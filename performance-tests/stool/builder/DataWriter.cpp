#include "DataWriter.h"

#include "DataWriterListener.h"

#include "dds/DCPS/transport/framework/TransportRegistry.h"

namespace Builder {

DataWriter::DataWriter(const DataWriterConfig& config, DataWriterReport& report, DDS::Publisher_var& publisher, const std::shared_ptr<TopicManager>& topics)
  : name_(config.name.in())
  , topic_name_(config.topic_name.in())
  , listener_type_name_(config.listener_type_name.in())
  , listener_status_mask_(config.listener_status_mask)
  , transport_config_name_(config.transport_config_name.in())
  , report_(report)
  , publisher_(publisher)
{
  //std::cout << "Creating datawriter: '" << name_ << "' with topic name '" << topic_name_ << "' and listener type name '" << listener_type_name_ << "'" << std::endl;

  auto topic_ptr = topics->get_topic_by_name(topic_name_);
  if (!topic_ptr) {
    std::stringstream ss;
    ss << "topic lookup failed in datawriter '" << name_ << "' for topic '" << topic_name_ << "'" << std::flush;
    throw std::runtime_error(ss.str());
  }
  topic_ = topic_ptr->get_dds_topic();

  DDS::DataWriterQos qos;
  publisher_->get_default_datawriter_qos(qos);

  //DurabilityQosPolicyMask durability;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, durability, kind);
  //DurabilityServiceQosPolicyMask durability_service;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, durability_service, service_cleanup_delay);
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, durability_service, history_kind);
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, durability_service, history_depth);
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, durability_service, max_samples);
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, durability_service, max_instances);
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, durability_service, max_samples_per_instance);
  //DeadlineQosPolicyMask deadline;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, deadline, period);
  //LatencyBudgetQosPolicyMask latency_budget;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, latency_budget, duration);
  //LivelinessQosPolicyMask liveliness;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, liveliness, kind);
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, liveliness, lease_duration);
  //ReliabilityQosPolicyMask reliability;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, reliability, kind);
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, reliability, max_blocking_time);
  //DestinationOrderQosPolicyMask destination_order;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, destination_order, kind);
  //HistoryQosPolicyMask history;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, history, kind);
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, history, depth);
  //ResourceLimitsQosPolicyMask resource_limits;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, resource_limits, max_samples);
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, resource_limits, max_instances);
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, resource_limits, max_samples_per_instance);
  //TransportPriorityQosPolicyMask transport_priority;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, transport_priority, value);
  //LifespanQosPolicyMask lifespan;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, lifespan, duration);
  //UserDataQosPolicyMask user_data;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, user_data, value);
  //OwnershipQosPolicyMask ownership;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, ownership, kind);
  //OwnershipStrengthQosPolicyMask ownership_strength;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, ownership_strength, value);
  //WriterDataLifecycleQosPolicyMask writer_data_lifecycle;
  APPLY_QOS_MASK(qos, config.qos, config.qos_mask, writer_data_lifecycle, autodispose_unregistered_instances);

  // Create Listener From Factory
  listener_ = DDS::DataWriterListener::_nil();
  if (!listener_type_name_.empty()) {
    listener_ = create_listener(listener_type_name_);
    if (!listener_) {
      std::stringstream ss;
      ss << "datareader listener creation failed for datawriter '" << name_ << "' with listener type name '" << listener_type_name_ << "'" << std::flush;
      throw std::runtime_error(ss.str());
    } else {
      DataWriterListener* savvy_listener_ = dynamic_cast<DataWriterListener*>(listener_.in());
      if (savvy_listener_) {
        savvy_listener_->set_datawriter(*this);
      }
    }
  }

  report_.enable_time = ZERO;
  report_.last_discovery_time = ZERO;

  report_.create_time = get_time();
  datawriter_ = publisher_->create_datawriter(topic_, qos, listener_, listener_status_mask_);
  if (CORBA::is_nil(datawriter_.in())) {
    throw std::runtime_error("datawriter creation failed");
  }

  DDS::PublisherQos publisher_qos;
  if (publisher_->get_qos(publisher_qos) == DDS::RETCODE_OK && publisher_qos.entity_factory.autoenable_created_entities == true) {
    report_.enable_time = report_.create_time;
  }

  if (!transport_config_name_.empty()) {
    std::cout << "Binding config for datawriter " << name_ << " (" << transport_config_name_ << ")"<< std::endl;
    TheTransportRegistry->bind_config(transport_config_name_.c_str(), datawriter_);
  }
}

DataWriter::~DataWriter() {
  if (datawriter_) {
  }
}

void DataWriter::enable() {
  datawriter_->enable();
}

}

