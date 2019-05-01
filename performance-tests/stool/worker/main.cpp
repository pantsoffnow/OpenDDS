#include "dds/DCPS/Service_Participant.h"

#include "ListenerFactory.h"

#include <dds/DCPS/transport/framework/TransportRegistry.h>

#include "StoolC.h"
#include "StoolTypeSupportImpl.h"

#include "TopicListener.h"
#include "DataReaderListener.h"
#include "DataWriterListener.h"
#include "SubscriberListener.h"
#include "PublisherListener.h"
#include "ParticipantListener.h"
#include "Process.h"

#include "json_2_builder.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <iomanip>
#include <condition_variable>

// MyTopicListener

class MyTopicListener : public Builder::TopicListener {
public:
  void on_inconsistent_topic(DDS::Topic_ptr /*the_topic*/, const DDS::InconsistentTopicStatus& /*status*/) {
    std::unique_lock<std::mutex> lock(mutex_);
    ++inconsistent_count_;
  }

  void set_topic(Builder::Topic& topic) {
    topic_ = &topic;
  }

protected:
  std::mutex mutex_;
  Builder::Topic* topic_{0};
  size_t inconsistent_count_{0};
};

// MyDataReaderListener

class MyDataReaderListener : public Builder::DataReaderListener {
public:

  MyDataReaderListener() {}
  MyDataReaderListener(size_t expected) : expected_count_(expected) {}

  void on_requested_deadline_missed(DDS::DataReader_ptr /*reader*/, const DDS::RequestedDeadlineMissedStatus& /*status*/) override {
  }

  void on_requested_incompatible_qos(DDS::DataReader_ptr /*reader*/, const DDS::RequestedIncompatibleQosStatus& /*status*/) override {
  }

  void on_sample_rejected(DDS::DataReader_ptr /*reader*/, const DDS::SampleRejectedStatus& /*status*/) override {
  }

  void on_liveliness_changed(DDS::DataReader_ptr /*reader*/, const DDS::LivelinessChangedStatus& /*status*/) override {
  }

  void on_data_available(DDS::DataReader_ptr /*reader*/) override {
  }

  void on_subscription_matched(DDS::DataReader_ptr /*reader*/, const DDS::SubscriptionMatchedStatus& status) override {
    std::unique_lock<std::mutex> lock(mutex_);
    if (expected_count_ != 0) {
      if (static_cast<size_t>(status.current_count) == expected_count_) {
        //std::cout << "MyDataReaderListener reached expected count!" << std::endl;
        if (datareader_) {
          datareader_->get_report().last_discovery_time = Builder::get_time();
        }
      }
    } else {
      if (static_cast<size_t>(status.current_count) > matched_count_) {
        if (datareader_) {
          datareader_->get_report().last_discovery_time = Builder::get_time();
        }
      }
    }
    matched_count_ = status.current_count;
  }

  void on_sample_lost(DDS::DataReader_ptr /*reader*/, const DDS::SampleLostStatus& /*status*/) override {
  }

  void set_datareader(Builder::DataReader& datareader) override {
    datareader_ = &datareader;
  }

protected:
  std::mutex mutex_;
  size_t expected_count_{0};
  size_t matched_count_{0};
  Builder::DataReader* datareader_{0};
};

// MySubscriberListener

class MySubscriberListener : public Builder::SubscriberListener {
public:

  // From DDS::DataReaderListener

  void on_requested_deadline_missed(DDS::DataReader_ptr /*reader*/, const DDS::RequestedDeadlineMissedStatus& /*status*/) override {
  }

  void on_requested_incompatible_qos(DDS::DataReader_ptr /*reader*/, const DDS::RequestedIncompatibleQosStatus& /*status*/) override {
  }

  void on_sample_rejected(DDS::DataReader_ptr /*reader*/, const DDS::SampleRejectedStatus& /*status*/) override {
  }

  void on_liveliness_changed(DDS::DataReader_ptr /*reader*/, const DDS::LivelinessChangedStatus& /*status*/) override {
  }

  void on_data_available(DDS::DataReader_ptr /*reader*/) override {
  }

  void on_subscription_matched(DDS::DataReader_ptr /*reader*/, const DDS::SubscriptionMatchedStatus& /*status*/) override {
  }

  void on_sample_lost(DDS::DataReader_ptr /*reader*/, const DDS::SampleLostStatus& /*status*/) override {
  }

  // From DDS::SubscriberListener

  void on_data_on_readers(DDS::Subscriber_ptr /*subs*/) override {
  }

  // From Builder::SubscriberListener

  void set_subscriber(Builder::Subscriber& subscriber) override {
    subscriber_ = &subscriber;
  }

protected:
  std::mutex mutex_;
  Builder::Subscriber* subscriber_{0};
};

// MyDataWriterListener

class MyDataWriterListener : public Builder::DataWriterListener {
public:

  MyDataWriterListener() {}
  MyDataWriterListener(size_t expected) : expected_count_(expected) {}

  void on_offered_deadline_missed(DDS::DataWriter_ptr /*writer*/, const DDS::OfferedDeadlineMissedStatus& /*status*/) override {
  }

  void on_offered_incompatible_qos(DDS::DataWriter_ptr /*writer*/, const DDS::OfferedIncompatibleQosStatus& /*status*/) override {
  }

  void on_liveliness_lost(DDS::DataWriter_ptr /*writer*/, const DDS::LivelinessLostStatus& /*status*/) override {
  }

  void on_publication_matched(DDS::DataWriter_ptr /*writer*/, const DDS::PublicationMatchedStatus& status) override {
    std::unique_lock<std::mutex> lock(mutex_);
    if (expected_count_ != 0) {
      if (static_cast<size_t>(status.current_count) == expected_count_) {
        //std::cout << "MyDataWriterListener reached expected count!" << std::endl;
        if (datawriter_) {
          datawriter_->get_report().last_discovery_time = Builder::get_time();
        }
      }
    } else {
      if (static_cast<size_t>(status.current_count) > matched_count_) {
        if (datawriter_) {
          datawriter_->get_report().last_discovery_time = Builder::get_time();
        }
      }
    }
    matched_count_ = status.current_count;
  }

  void set_datawriter(Builder::DataWriter& datawriter) override {
    datawriter_ = &datawriter;
  }

protected:
  std::mutex mutex_;
  size_t expected_count_{0};
  size_t matched_count_{0};
  Builder::DataWriter* datawriter_{0};
};

// MyPublisherListener

class MyPublisherListener : public Builder::PublisherListener {
public:

  // From DDS::DataWriterListener

  void on_offered_deadline_missed(DDS::DataWriter_ptr /*writer*/, const DDS::OfferedDeadlineMissedStatus& /*status*/) override {
  }

  void on_offered_incompatible_qos(DDS::DataWriter_ptr /*writer*/, const DDS::OfferedIncompatibleQosStatus& /*status*/) override {
  }

  void on_liveliness_lost(DDS::DataWriter_ptr /*writer*/, const DDS::LivelinessLostStatus& /*status*/) override {
  }

  void on_publication_matched(DDS::DataWriter_ptr /*writer*/, const DDS::PublicationMatchedStatus& /*status*/) override {
  }

  // From DDS::PublisherListener

  // From Stool::PublisherListener

  void set_publisher(Builder::Publisher& publisher) override {
    publisher_ = &publisher;
  }

protected:
  std::mutex mutex_;
  Builder::Publisher* publisher_{0};
};

// MyParticipantListener

class MyParticipantListener : public Builder::ParticipantListener {
public:

  // From DDS::DataWriterListener

  void on_offered_deadline_missed(DDS::DataWriter_ptr /*writer*/, const DDS::OfferedDeadlineMissedStatus& /*status*/) {
  }

  void on_offered_incompatible_qos(DDS::DataWriter_ptr /*writer*/, const DDS::OfferedIncompatibleQosStatus& /*status*/) {
  }

  void on_liveliness_lost(DDS::DataWriter_ptr /*writer*/, const DDS::LivelinessLostStatus& /*status*/) {
  }

  void on_publication_matched(DDS::DataWriter_ptr /*writer*/, const DDS::PublicationMatchedStatus& /*status*/) {
  }

  void on_requested_deadline_missed(DDS::DataReader_ptr /*reader*/, const DDS::RequestedDeadlineMissedStatus& /*status*/) {
  }

  void on_requested_incompatible_qos(DDS::DataReader_ptr /*reader*/, const DDS::RequestedIncompatibleQosStatus& /*status*/) {
  }

  // From DDS::SubscriberListener

  void on_data_on_readers(DDS::Subscriber_ptr /*subscriber*/) {
  }

  // From DDS::DataReaderListener

  void on_sample_rejected(DDS::DataReader_ptr /*reader*/, const DDS::SampleRejectedStatus& /*status*/) {
  }

  void on_liveliness_changed(DDS::DataReader_ptr /*reader*/, const DDS::LivelinessChangedStatus& /*status*/) {
  }

  void on_data_available(DDS::DataReader_ptr /*reader*/) {
  }

  void on_subscription_matched(DDS::DataReader_ptr /*reader*/, const DDS::SubscriptionMatchedStatus& /*status*/) {
  }

  void on_sample_lost(DDS::DataReader_ptr /*reader*/, const DDS::SampleLostStatus& /*status*/) {
  }

  void on_inconsistent_topic(DDS::Topic_ptr /*the_topic*/, const DDS::InconsistentTopicStatus& /*status*/) {
  }

  // From Builder::ParticipantListener

  void set_participant(Builder::Participant& participant) override {
    participant_ = &participant;
  }

protected:
  std::mutex mutex_;
  Builder::Participant* participant_{0};
};

// Main

int ACE_TMAIN(int argc, ACE_TCHAR* argv[]) {

  if (argc < 2) {
    std::cerr << "Configuration file expected as second argument." << std::endl;
    return 1;
  }

  std::ifstream ifs(argv[1]);
  if (!ifs.good()) {
    std::cerr << "Unable to open configuration file: '" << argv[1] << "'" << std::endl;
    return 2;
  }

  using Builder::ZERO;

  Stool::WorkerConfig config;

  config.enable_time = ZERO;
  config.start_time = ZERO;
  config.stop_time = ZERO;

  if (!json_2_builder(ifs, config)) {
    std::cerr << "Unable to parse configuration file: '" << argv[1] << "'" << std::endl;
    return 3;
  }

  /* Manually building the config object

  config.config_sections.length(1);
  config.config_sections[0].name = "common";
  config.config_sections[0].properties.length(2);
  config.config_sections[0].properties[0].name = "DCPSSecurity";
  config.config_sections[0].properties[0].value = "0";
  config.config_sections[0].properties[1].name = "DCPSDebugLevel";
  config.config_sections[0].properties[1].value = "0";

  config.discoveries.length(1);
  config.discoveries[0].type = "rtps";
  config.discoveries[0].name = "stool_test_rtps";
  config.discoveries[0].domain = 7;

  const size_t ips_per_proc_max = 10;
  const size_t subs_per_ip_max = 5;
  const size_t pubs_per_ip_max = 5;
  const size_t drs_per_sub_max = 10;
  const size_t dws_per_pub_max = 10;

  const size_t expected_datareader_matches = ips_per_proc_max * pubs_per_ip_max * dws_per_pub_max;
  const size_t expected_datawriter_matches = ips_per_proc_max * subs_per_ip_max * drs_per_sub_max;

  config.instances.length(ips_per_proc_max);
  config.participants.length(ips_per_proc_max);

  for (size_t ip = 0; ip < ips_per_proc_max; ++ip) {
    std::stringstream instance_name_ss;
    instance_name_ss << "transport_" << ip << std::flush;
    config.instances[ip].type = "rtps_udp";
    config.instances[ip].name = instance_name_ss.str().c_str();
    config.instances[ip].domain = 7;
    std::stringstream participant_name_ss;
    participant_name_ss << "participant_" << ip << std::flush;
    config.participants[ip].name = participant_name_ss.str().c_str();
    config.participants[ip].domain = 7;
    config.participants[ip].listener_type_name = "stool_partl";
    config.participants[ip].listener_status_mask = OpenDDS::DCPS::DEFAULT_STATUS_MASK;
    //config.participants[ip].type_names.length(1);
    //config.participants[ip].type_names[0] = "Stool::Data";
    config.participants[ip].transport_config_name = instance_name_ss.str().c_str();

    config.participants[ip].qos.entity_factory.autoenable_created_entities = false;
    config.participants[ip].qos_mask.entity_factory.has_autoenable_created_entities = false;

    config.participants[ip].topics.length(1);
    std::stringstream topic_name_ss;
    topic_name_ss << "topic" << std::flush;
    config.participants[ip].topics[0].name = topic_name_ss.str().c_str();
    //config.participants[ip].topics[0].type_name = "Stool::Data";
    config.participants[ip].topics[0].listener_type_name = "stool_tl";
    config.participants[ip].topics[0].listener_status_mask = OpenDDS::DCPS::DEFAULT_STATUS_MASK;

    config.participants[ip].subscribers.length(subs_per_ip_max);
    for (size_t sub = 0; sub < subs_per_ip_max; ++sub) {
      std::stringstream subscriber_name_ss;
      subscriber_name_ss << "subscriber_" << ip << "_" << sub << std::flush;
      config.participants[ip].subscribers[sub].name = subscriber_name_ss.str().c_str();
      config.participants[ip].subscribers[sub].listener_type_name = "stool_sl";
      config.participants[ip].subscribers[sub].listener_status_mask = OpenDDS::DCPS::DEFAULT_STATUS_MASK;

      config.participants[ip].subscribers[sub].datareaders.length(drs_per_sub_max);
      for (size_t dr = 0; dr < drs_per_sub_max; ++dr) {
        std::stringstream datareader_name_ss;
        datareader_name_ss << "datareader_" << ip << "_" << sub << "_" << dr << std::flush;
        config.participants[ip].subscribers[sub].datareaders[dr].name = datareader_name_ss.str().c_str();
        config.participants[ip].subscribers[sub].datareaders[dr].topic_name = topic_name_ss.str().c_str();
        config.participants[ip].subscribers[sub].datareaders[dr].listener_type_name = "stool_drl";
        config.participants[ip].subscribers[sub].datareaders[dr].listener_status_mask = OpenDDS::DCPS::DEFAULT_STATUS_MASK;

        config.participants[ip].subscribers[sub].datareaders[dr].qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
        config.participants[ip].subscribers[sub].datareaders[dr].qos_mask.reliability.has_kind = true;
      }
    }
    config.participants[ip].publishers.length(pubs_per_ip_max);
    for (size_t pub = 0; pub < pubs_per_ip_max; ++pub) {
      std::stringstream publisher_name_ss;
      publisher_name_ss << "publisher_" << ip << "_" << pub << std::flush;
      config.participants[ip].publishers[pub].name = publisher_name_ss.str().c_str();
      config.participants[ip].publishers[pub].listener_type_name = "stool_pl";
      config.participants[ip].publishers[pub].listener_status_mask = OpenDDS::DCPS::DEFAULT_STATUS_MASK;

      config.participants[ip].publishers[pub].datawriters.length(dws_per_pub_max);
      for (size_t dw = 0; dw < dws_per_pub_max; ++dw) {
        std::stringstream datawriter_name_ss;
        datawriter_name_ss << "datawriter_" << ip << "_" << pub << "_" << dw << std::flush;
        config.participants[ip].publishers[pub].datawriters[dw].name = datawriter_name_ss.str().c_str();
        config.participants[ip].publishers[pub].datawriters[dw].topic_name = topic_name_ss.str().c_str();
        config.participants[ip].publishers[pub].datawriters[dw].listener_type_name = "stool_dwl";
        config.participants[ip].publishers[pub].datawriters[dw].listener_status_mask = OpenDDS::DCPS::DEFAULT_STATUS_MASK;
      }
    }
  }
  */

  // Register some Stool-specific types
  Builder::TypeSupportRegistry::TypeSupportRegistration data_registration(new Stool::DataTypeSupportImpl());
  //Builder::TypeSupportRegistry::TypeSupportRegistration process_config_registration(new Stool::ProcessConfigTypeSupportImpl());

  // Register some Stool-specific listener factories
  Builder::ListenerFactory<DDS::TopicListener>::Registration topic_registration("stool_tl", [](){ return DDS::TopicListener_var(new MyTopicListener()); });
  Builder::ListenerFactory<DDS::DataReaderListener>::Registration datareader_registration("stool_drl", [&](){ return DDS::DataReaderListener_var(new MyDataReaderListener()); });
  Builder::ListenerFactory<DDS::SubscriberListener>::Registration subscriber_registration("stool_sl", [](){ return DDS::SubscriberListener_var(new MySubscriberListener()); });
  Builder::ListenerFactory<DDS::DataWriterListener>::Registration datawriter_registration("stool_dwl", [&](){ return DDS::DataWriterListener_var(new MyDataWriterListener()); });
  Builder::ListenerFactory<DDS::PublisherListener>::Registration publisher_registration("stool_pl", [](){ return DDS::PublisherListener_var(new MyPublisherListener()); });
  Builder::ListenerFactory<DDS::DomainParticipantListener>::Registration participant_registration("stool_partl", [](){ return DDS::DomainParticipantListener_var(new MyParticipantListener()); });

  // Timestamps used to measure method call durations
  Builder::TimeStamp process_construction_begin_time = ZERO, process_construction_end_time = ZERO;
  Builder::TimeStamp process_enable_begin_time = ZERO, process_enable_end_time = ZERO;
  Builder::TimeStamp process_start_begin_time = ZERO, process_start_end_time = ZERO;
  Builder::TimeStamp process_stop_begin_time = ZERO, process_stop_end_time = ZERO;
  Builder::TimeStamp process_destruction_begin_time = ZERO, process_destruction_end_time = ZERO;

  Builder::ProcessReport report;

  try {
    std::string line;
    std::condition_variable cv;
    std::mutex cv_mutex;

    std::cout << "Beginning process construction / entity creation." << std::endl;

    process_construction_begin_time = Builder::get_time();
    Builder::Process process(config.process);
    process_construction_end_time = Builder::get_time();

    std::cout << std::endl << "Process construction / entity creation complete." << std::endl << std::endl;

    if (config.enable_time == ZERO) {
      std::cout << "No test enable time specified. Press any key to enable process entities." << std::endl;
      std::getline(std::cin, line);
    } else {
      if (config.enable_time < ZERO) {
        auto dur = -get_duration(config.enable_time);
        std::unique_lock<std::mutex> lock(cv_mutex);
        while (cv.wait_for(lock, dur) != std::cv_status::timeout) {}
      } else {
        auto timeout_time = std::chrono::system_clock::time_point(get_duration(config.enable_time));
        std::unique_lock<std::mutex> lock(cv_mutex);
        while (cv.wait_until(lock, timeout_time) != std::cv_status::timeout) {}
      }
    }

    std::cout << "Enabling DDS entities (if not already enabled)." << std::endl;

    process_enable_begin_time = Builder::get_time();
    process.enable_dds_entities();
    process_enable_end_time = Builder::get_time();

    std::cout << "DDS entities enabled." << std::endl << std::endl;

    if (config.start_time == ZERO) {
      std::cout << "No test start time specified. Press any key to start process testing." << std::endl;
      std::getline(std::cin, line);
    } else {
      if (config.start_time < ZERO) {
        auto dur = -get_duration(config.start_time);
        std::unique_lock<std::mutex> lock(cv_mutex);
        while (cv.wait_for(lock, dur) != std::cv_status::timeout) {}
      } else {
        auto timeout_time = std::chrono::system_clock::time_point(get_duration(config.start_time));
        std::unique_lock<std::mutex> lock(cv_mutex);
        while (cv.wait_until(lock, timeout_time) != std::cv_status::timeout) {}
      }
    }

    std::cout << "Starting process tests." << std::endl;

    process_start_begin_time = Builder::get_time();
    //process.start();
    process_start_end_time = Builder::get_time();

    std::cout << "Process tests started." << std::endl << std::endl;

    if (config.stop_time == ZERO) {
      std::cout << "No stop time specified. Press any key to stop process testing." << std::endl;
      std::getline(std::cin, line);
    } else {
      if (config.stop_time < ZERO) {
        auto dur = -get_duration(config.stop_time);
        std::unique_lock<std::mutex> lock(cv_mutex);
        while (cv.wait_for(lock, dur) != std::cv_status::timeout) {}
      } else {
        auto timeout_time = std::chrono::system_clock::time_point(get_duration(config.stop_time));
        std::unique_lock<std::mutex> lock(cv_mutex);
        while (cv.wait_until(lock, timeout_time) != std::cv_status::timeout) {}
      }
    }

    std::cout << "Stopping process tests." << std::endl;

    process_stop_begin_time = Builder::get_time();
    //process.stop();
    process_stop_end_time = Builder::get_time();

    std::cout << "Process tests stopped." << std::endl << std::endl;

    report = process.get_report();

    std::cout << "Beginning process destruction / entity deletion." << std::endl;

    process_destruction_begin_time = Builder::get_time();
  } catch (const std::exception& e) {
    std::cerr << "Exception caught trying to build process object: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Unknown exception caught trying to build process object" << std::endl;
    return 1;
  }
  process_destruction_end_time = Builder::get_time();

  std::cout << "Process destruction / entity deletion complete." << std::endl << std::endl;

  // Some preliminary measurements and reporting (eventually will shift to another process?)
  Builder::TimeStamp max_discovery_time_delta = ZERO;

  size_t undermatched_readers = 0;
  size_t undermatched_writers = 0;

  for (CORBA::ULong i = 0; i < report.participants.length(); ++i) {
    for (CORBA::ULong j = 0; j < report.participants[i].subscribers.length(); ++j) {
      for (CORBA::ULong k = 0; k < report.participants[i].subscribers[j].datareaders.length(); ++k) {
        if (ZERO < report.participants[i].subscribers[j].datareaders[k].enable_time && ZERO < report.participants[i].subscribers[j].datareaders[k].last_discovery_time) {
          auto delta = report.participants[i].subscribers[j].datareaders[k].last_discovery_time - report.participants[i].subscribers[j].datareaders[k].enable_time;
          if (max_discovery_time_delta < delta) {
            max_discovery_time_delta = delta;
          }
        } else {
          ++undermatched_readers;
        }
      }
    }
    for (CORBA::ULong j = 0; j < report.participants[i].publishers.length(); ++j) {
      for (CORBA::ULong k = 0; k < report.participants[i].publishers[j].datawriters.length(); ++k) {
        if (ZERO < report.participants[i].publishers[j].datawriters[k].enable_time && ZERO < report.participants[i].publishers[j].datawriters[k].last_discovery_time) {
          auto delta = report.participants[i].publishers[j].datawriters[k].last_discovery_time - report.participants[i].publishers[j].datawriters[k].enable_time;
          if (max_discovery_time_delta < delta) {
            max_discovery_time_delta = delta;
          }
        } else {
          ++undermatched_writers;
        }
      }
    }
  }

  std::cout << "undermatched readers: " << undermatched_readers << ", undermatched writers: " << undermatched_writers << std::endl << std::endl;

  std::cout << "construction_time: " << process_construction_end_time - process_construction_begin_time << std::endl;
  std::cout << "enable_time: " << process_enable_end_time - process_enable_begin_time << std::endl;
  //std::cout << "start_time: " << process_start_end_time - process_start_begin_time << std::endl;
  //std::cout << "stop_time: " << process_stop_end_time - process_stop_begin_time << std::endl;
  std::cout << "destruction_time: " << process_destruction_end_time - process_destruction_begin_time << std::endl;
  std::cout << "max_discovery_time_delta: " << max_discovery_time_delta << std::endl;

  return 0;
}

