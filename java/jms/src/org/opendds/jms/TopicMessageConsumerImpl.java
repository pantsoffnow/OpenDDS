/*
 * $Id$
 */

package org.opendds.jms;

import java.util.List;

import javax.jms.Destination;
import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageConsumer;
import javax.jms.MessageListener;
import javax.jms.Session;

import DDS.ALIVE_INSTANCE_STATE;
import DDS.Condition;
import DDS.ConditionSeqHolder;
import DDS.DATAREADER_QOS_DEFAULT;
import DDS.DATA_AVAILABLE_STATUS;
import DDS.DURATION_INFINITY_NSEC;
import DDS.DURATION_INFINITY_SEC;
import DDS.DataReader;
import DDS.DataReaderQosHolder;
import DDS.DomainParticipant;
import DDS.Duration_t;
import DDS.GuardCondition;
import DDS.NEW_VIEW_STATE;
import DDS.NOT_READ_SAMPLE_STATE;
import DDS.RETCODE_OK;
import DDS.RETCODE_TIMEOUT;
import DDS.ReadCondition;
import DDS.ReadConditionHelper;
import DDS.SampleInfo;
import DDS.SampleInfoSeqHolder;
import DDS.Subscriber;
import DDS.Topic;
import DDS.WaitSet;
import DDS.READ_SAMPLE_STATE;
import DDS.NOT_NEW_VIEW_STATE;
import OpenDDS.JMS.MessagePayload;
import OpenDDS.JMS.MessagePayloadDataReader;
import OpenDDS.JMS.MessagePayloadDataReaderHelper;
import OpenDDS.JMS.MessagePayloadSeqHolder;

import static org.opendds.jms.ConsumerMessageFactory.buildMessageFromPayload;
import org.opendds.jms.common.lang.Objects;
import org.opendds.jms.common.lang.Strings;

/**
 * @author  Steven Stallion
 * @version $Revision$
 */
public class TopicMessageConsumerImpl implements MessageConsumer {
    private Destination destination;
    private String messageSelector;
    private MessageListener messageListener;
    private boolean noLocal;

    // DDS related stuff
    private Subscriber subscriber;
    private DomainParticipant participant;
    private MessagePayloadDataReader messagePayloadDataReader;

    private boolean closed;
    private final WaitSet waitSet;
    private final GuardCondition closeToken;

    // JMS 1.1, 4.4.11,
    private SessionImpl sessionImpl;
    private List<DataReaderHandlePair> toBeRecovered;

    public TopicMessageConsumerImpl(Destination destination, String messageSelector, boolean noLocal, Subscriber subscriber, DomainParticipant participant, SessionImpl sessionImpl) throws JMSException {
        Objects.ensureNotNull(subscriber);
        Objects.ensureNotNull(participant);

        this.destination = destination;
        this.subscriber = subscriber;
        this.participant = participant;
        this.messageSelector = messageSelector;
        this.noLocal = noLocal;

        this.closed = false;
        this.messagePayloadDataReader = fromDestination(destination, subscriber, participant);
        this.closeToken = new GuardCondition();
        this.closeToken.set_trigger_value(false);
        this.waitSet = new WaitSet();
        this.waitSet.attach_condition(closeToken);

        this.sessionImpl = sessionImpl;
        this.toBeRecovered = null;
    }

    private MessagePayloadDataReader fromDestination(Destination destination, Subscriber subscriber, DomainParticipant participant) throws JMSException {
        DDS.Topic ddsTopic = extractDDSTopicFromDestination(destination, participant);
        DataReaderQosHolder qosHolder = new DataReaderQosHolder(DATAREADER_QOS_DEFAULT.get());
        subscriber.get_default_datareader_qos(qosHolder);
        DataReader reader = subscriber.create_datareader(ddsTopic, qosHolder.value, null);
        return MessagePayloadDataReaderHelper.narrow(reader);
    }

    private Topic extractDDSTopicFromDestination(Destination destination, DomainParticipant participant) throws JMSException {
        // TODO placeholder, to be elaborated
        TopicImpl topicImpl = (TopicImpl) destination;
        return topicImpl.createTopic(participant);
    }

    public String getMessageSelector() throws JMSException {
        return Strings.isEmpty(messageSelector) ? null : messageSelector;
    }

    public MessageListener getMessageListener() {
        return messageListener;
    }

    public void setMessageListener(MessageListener messageListener) throws JMSException {
        checkClosed();
        if (messageListener == null) {
            messagePayloadDataReader.set_listener(null, 0);
        } else {
            messagePayloadDataReader.set_listener(new ConsumerDataReaderListener(this, sessionImpl), DATA_AVAILABLE_STATUS.value);
        }
        this.messageListener = messageListener;
    }

    public Message receive() throws JMSException {
        checkClosed();
        Duration_t duration = new Duration_t(DURATION_INFINITY_SEC.value, DURATION_INFINITY_NSEC.value);
        return doRecoverOrReceive(duration);
    }

    public Message receive(long timeout) throws JMSException {
        checkClosed();
        Duration_t duration = new Duration_t();
        if (timeout < 0) throw new IllegalArgumentException("The timeout specified is negative: " + timeout);
        if (timeout == 0) {
            duration.sec = DURATION_INFINITY_SEC.value;
            duration.nanosec = DURATION_INFINITY_NSEC.value;
        } else {
            duration.sec = (int) timeout / 1000;
            duration.nanosec = ((int) (timeout % 1000)) * 1000;
        }
        return doRecoverOrReceive(duration);
    }

    public Message receiveNoWait() throws JMSException {
        checkClosed();
        Duration_t duration = new Duration_t(0, 0);
        return doRecoverOrReceive(duration);
    }

    private Message doRecoverOrReceive(Duration_t duration) throws JMSException {
        if (toBeRecovered != null && !toBeRecovered.isEmpty()) {
            return doRecoverSync();
        } else {
            return doReceive(duration);
        }
    }

    private Message doReceive(Duration_t duration) throws JMSException {
        ReadCondition readCondition = messagePayloadDataReader.create_readcondition(NOT_READ_SAMPLE_STATE.value,
                NEW_VIEW_STATE.value, ALIVE_INSTANCE_STATE.value);
        int rc = waitSet.attach_condition(readCondition);
        if (rc != RETCODE_OK.value) {
            throw new JMSException("Cannot attach readCondition to OpenDDS WaitSet.");
        }

        ConditionSeqHolder conditions = new ConditionSeqHolder(new Condition[0]);

        while (true) {
            rc = waitSet.wait(conditions, duration);
            if (rc == RETCODE_TIMEOUT.value) {
                waitSet.detach_condition(readCondition);
                messagePayloadDataReader.delete_readcondition(readCondition);
                return null;
            } else if (rc != RETCODE_OK.value) {
                waitSet.detach_condition(readCondition);
                messagePayloadDataReader.delete_readcondition(readCondition);
                throw new JMSException("WaitSet returned from wait() .");
            }

            for (int i = 0; i < conditions.value.length; i++) {
                final Condition innerCondition = conditions.value[i];
                if (innerCondition._is_equivalent(readCondition)) {
                    ReadCondition innerReadCondition = ReadConditionHelper.narrow(innerCondition);
                    MessagePayloadSeqHolder payloads = new MessagePayloadSeqHolder(new MessagePayload[0]);
                    SampleInfoSeqHolder infos = new SampleInfoSeqHolder(new SampleInfo[0]);
                    rc = messagePayloadDataReader.read_w_condition(payloads, infos, 1, innerReadCondition);
                    if (rc != RETCODE_OK.value) continue;

                    // We should see one sample
                    MessagePayload messagePayload = payloads.value[0];
                    SampleInfo sampleInfo = infos.value[0];
                    int handle = sampleInfo.instance_handle;
                    waitSet.detach_condition(readCondition);
                    messagePayloadDataReader.delete_readcondition(readCondition);
                    AbstractMessageImpl message = buildMessageFromPayload(messagePayload, handle, sessionImpl);
                    DataReaderHandlePair dataReaderHandlePair = new DataReaderHandlePair(messagePayloadDataReader, handle);
                    sessionImpl.addToUnacknowledged(dataReaderHandlePair, this);
                    if (sessionImpl.getAcknowledgeMode() != Session.CLIENT_ACKNOWLEDGE) {
                        sessionImpl.doAcknowledge();
                    }
                    return message;
                } else if (innerCondition._is_equivalent(closeToken)) {
                    // close() is called from another thread
                    waitSet.detach_condition(readCondition);
                    messagePayloadDataReader.delete_readcondition(readCondition);
                    return null;
                }
            }
        }
    }

    public void close() throws JMSException {
        if (closed) return;
        if (messagePayloadDataReader != null) {
            closeToken.set_trigger_value(true);
            subscriber.delete_datareader(messagePayloadDataReader);
        }
        this.closed = true;
    }

    private void checkClosed() {
        // JMS 1.1, 4.4.1
        if (closed) throw new IllegalStateException("This MessageConsumer is closed.");
    }

    void doRecover(List<DataReaderHandlePair> dataReaderHandlePairs) {
        if (messageListener != null) {
            doRecoverAsync(dataReaderHandlePairs);
        } else {
            toBeRecovered = dataReaderHandlePairs;
        }
    }

    private void doRecoverAsync(List<DataReaderHandlePair> dataReaderHandlePairs) {
        for (DataReaderHandlePair pair : dataReaderHandlePairs) {
            final MessagePayloadDataReader reader = pair.getDataReader();
            final int handle = pair.getInstanceHandle();
            MessagePayloadSeqHolder payloads = new MessagePayloadSeqHolder(new MessagePayload[0]);
            SampleInfoSeqHolder infos = new SampleInfoSeqHolder(new SampleInfo[0]);
            int rc = reader.read_instance(payloads, infos, 1, handle, READ_SAMPLE_STATE.value, NOT_NEW_VIEW_STATE.value, ALIVE_INSTANCE_STATE.value);
            if (rc != RETCODE_OK.value) continue;

            MessagePayload messagePayload = payloads.value[0];
            SampleInfo sampleInfo = infos.value[0];
            AbstractMessageImpl message = buildMessageFromPayload(messagePayload, handle, sessionImpl);
            message.setJMSRedelivered(true);
            sessionImpl.getMessageDeliveryExecutorService().execute(new MessageDispatcher(message, pair, this, sessionImpl));
        }
    }

    private Message doRecoverSync() {
        while (true) {
            final DataReaderHandlePair pair = toBeRecovered.remove(0);
            final MessagePayloadDataReader reader = pair.getDataReader();
            final int handle = pair.getInstanceHandle();
            MessagePayloadSeqHolder payloads = new MessagePayloadSeqHolder(new MessagePayload[0]);
            SampleInfoSeqHolder infos = new SampleInfoSeqHolder(new SampleInfo[0]);
            int rc = reader.read_instance(payloads, infos, 1, handle, READ_SAMPLE_STATE.value, NOT_NEW_VIEW_STATE.value, ALIVE_INSTANCE_STATE.value);
            if (rc != RETCODE_OK.value) continue;
            MessagePayload messagePayload = payloads.value[0];
            SampleInfo sampleInfo = infos.value[0];
            AbstractMessageImpl message = buildMessageFromPayload(messagePayload, handle, sessionImpl);
            message.setJMSRedelivered(true);
            sessionImpl.addToUnacknowledged(pair, this);
            if (sessionImpl.getAcknowledgeMode() != Session.CLIENT_ACKNOWLEDGE) {
                sessionImpl.doAcknowledge();
            }
            return message;
        }
    }
}
