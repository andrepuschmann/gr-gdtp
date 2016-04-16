/* -*- c++ -*- */
/*
 * Copyright 2015, Andre Puschmann <andre.puschmann@tu-ilmenau.de>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <gnuradio/block_detail.h>

#include <iostream>
#include <string>

#include "libgdtp/libgdtp.h"
#include "gdtp_wrapper_impl.h"


/**
 *  The basic skeleton of this block is based on the RIME stack block written
 *  by Christoph Leitner as part of gr-ieee802-15-4.
 */

namespace gr {
  namespace gdtp {

    gdtp_wrapper::sptr
    gdtp_wrapper::make(bool debug, uint64_t src_addr, uint64_t dest_addr, const std::vector<int> &reliable, std::string addr_mode, std::string addr_src, int ack_timeout, int max_retry, int max_seq_no, std::string scheduler, int num_flows)
    {
      return gnuradio::get_initial_sptr
        (new gdtp_wrapper_impl(debug, src_addr, dest_addr, reliable, addr_mode, addr_src, ack_timeout, max_retry, max_seq_no, scheduler, num_flows));
    }

    /*
     * The private constructor
     */
    gdtp_wrapper_impl::gdtp_wrapper_impl(bool debug, uint64_t src_addr, uint64_t dest_addr, const std::vector<int> &reliable, std::string addr_mode, std::string addr_src, int ack_timeout, int max_retry, int max_seq_no, std::string scheduler, int num_flows)
      : gr::block("gdtp_wrapper",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0)),
        debug_(debug),
        src_address_(src_addr),
        dest_address_(dest_addr),
        addr_mode_(addr_mode),
        addr_src_(addr_src),
        reliable_(reliable),
        ack_timeout_(ack_timeout),
        max_retry_(max_retry),
        max_seq_no_(max_seq_no),
        scheduler_(scheduler),
        num_flows(num_flows)
    {
        // register data message ports
        std::string inport_name("fromMAC");
        std::string outport_name("toMAC");
        mac_in = pmt::mp(inport_name);
        mac_out = pmt::mp(outport_name);

        message_port_register_in(mac_in);
        set_msg_handler(mac_in, boost::bind(&gdtp_wrapper_impl::unpack, this, inport_name, _1));
        message_port_register_out(mac_out);

        // register port for FER
        message_port_register_out(pmt::mp("fer"));

        // configure and init gdtp instance
        gdtp_ = std::unique_ptr<libgdtp::Gdtp>(new libgdtp::Gdtp());
        gdtp_->set_scheduler_type(scheduler_);
        gdtp_->set_default_source_address(src_address_);
        gdtp_->set_default_destination_address(dest_address_);
        gdtp_->initialize();

        // register flow message ports
        register_flows(num_flows, "flowin", "flowout");

        // start transmit thread
        threads_.push_back(new boost::thread(boost::bind(&gdtp_wrapper_impl::tx_handler, this)));
    }


    void gdtp_wrapper_impl::register_flows(int num_flows, const char *inport_base, const char *outport_base)
    {
        std::cout << "Registering " << num_flows << " flows." << std::endl;

        if(std::strlen(inport_base) == 0 || std::strlen(outport_base) == 0) {
            throw std::invalid_argument("No in/outport name specified.");
        }

        for (int i = 0; i < num_flows; i++) {
            std::string inport_name(inport_base);
            std::string outport_name(outport_base);
            if (num_flows != 1) {
                inport_name += std::to_string(i);
                outport_name += std::to_string(i);
            }

            pmt::pmt_t inport = pmt::mp(inport_name);
            pmt::pmt_t outport = pmt::mp(outport_name);
            message_port_register_out(outport);
            message_port_register_in(inport);

            // construct properties
            FlowProperties props((reliable_.at(i) != 0 ? RELIABLE : UNRELIABLE),
                            99,
                           (addr_mode_ == "implicit" ? IMPLICIT : EXPLICIT),
                            max_seq_no_,
                            ack_timeout_,
                            max_retry_,
                            addr_src_);

            // allocate flow
            FlowId id = gdtp_->allocate_flow(i, props);
            gdtpPorts_[inport_name] = id;

            // setup handler for "incoming" msgs
            set_msg_handler(inport, boost::bind(&gdtp_wrapper_impl::pack, this, inport_name, id, _1));

            // create thread for outgoing msgs for this flow, i.e., when libgdtp has frames
            threads_.push_back(new boost::thread(boost::bind(&gdtp_wrapper_impl::flowout_handler, this, outport_name, i)));
        }
    }

    void gdtp_wrapper_impl::pack(std::string inport_name, FlowId id, pmt::pmt_t msg)
    {
        if (debug_) std::cout << "pack called on portname " << inport_name << std::endl;
        size_t payload_len;
        const char *payload;

        if (pmt::is_eof_object(msg)) {
            message_port_pub(mac_out, pmt::PMT_EOF);
            detail().get()->set_done(true);
            return;
        } else if (pmt::is_symbol(msg)) {
            std::string str;
            str = pmt::symbol_to_string(msg);
            payload_len = str.length();
            payload = str.data();
        } else if(pmt::is_pair(msg)) {
            payload_len = pmt::blob_length(pmt::cdr(msg));
            payload = reinterpret_cast<const char *>(pmt::blob_data(pmt::cdr(msg)));
        } else {
            throw std::invalid_argument("GDTP expects PDUs or strings");
            return;
        }

        std::shared_ptr<Data> sdu = std::make_shared<Data>(payload, payload + payload_len);
        gdtp_->handle_data_from_above(sdu, id);
    }


    void gdtp_wrapper_impl::unpack(std::string inport_name, pmt::pmt_t msg)
    {
        if (debug_) std::cout << "Unpack called on port " << inport_name << std::endl;

        if (pmt::is_pair(msg)) {
            size_t msg_len = pmt::blob_length(pmt::cdr(msg));
            const char* msdu = reinterpret_cast<const char *>(pmt::blob_data(pmt::cdr(msg)));
            Data frame(msdu, msdu + msg_len);
            if (debug_) std::cout << "Receiving PDU with size " << frame.size() << std::endl;
            gdtp_->handle_data_from_below(DEFAULT_BELOW_PORT_ID, frame);
        } else {
            throw std::runtime_error("PMT must be blob");
        }
    }


    void gdtp_wrapper_impl::tx_handler()
    {
        std::cout << "transmit thread started." << std::endl;

        try
        {
            while (true)
            {
                boost::this_thread::interruption_point();
                Data pdu;
                // this call may block if no frames are present
                gdtp_->get_data_for_below(DEFAULT_BELOW_PORT_ID, pdu);
                if (debug_) std::cout << "Transmitting PDU with size " << pdu.size() << std::endl;

                pmt::pmt_t msg = pmt::make_blob(pdu.data(), pdu.size());
                message_port_pub(mac_out, pmt::cons(pmt::PMT_NIL, msg));

                // FIXME: wait for txover event
                gdtp_->set_data_transmitted(DEFAULT_BELOW_PORT_ID);
            }
        }
        catch(boost::thread_interrupted)
        {
            std::cout << "Tx thread interrupted (id: " << boost::this_thread::get_id() << ")." << std::endl;
        }
    }


    void gdtp_wrapper_impl::flowout_handler(std::string outport_name, FlowId id)
    {
        std::cout << "flowout handler thread for " << outport_name << " with id " << id << " started." << std::endl;

        try
        {
            while (true)
            {
                boost::this_thread::interruption_point();

                std::shared_ptr<Data> sdu = gdtp_->get_data_for_above(id);
                if (debug_) std::cout << "received: " << sdu->size() << " byte." << std::endl;
                pmt::pmt_t msg = pmt::make_blob(sdu->data(), sdu->size());
                message_port_pub(pmt::mp(outport_name), pmt::cons(pmt::PMT_NIL, msg));

                // publish FER estimate
                FlowStats stats = gdtp_->get_stats(id);
                pmt::pmt_t pdu = pmt::make_f32vector(1, stats.arq.fer * 100);
                message_port_pub(pmt::mp("fer"), pmt::cons( pmt::PMT_NIL, pdu ));
            }
        }
        catch(boost::thread_interrupted)
        {
            std::cout << "flowout thread for " << outport_name << " interrupted (id: " << boost::this_thread::get_id() << ")." << std::endl;
        }
    }


    /*
     * Our virtual destructor.
     */
    gdtp_wrapper_impl::~gdtp_wrapper_impl()
    {
        // stop threads
        boost::ptr_vector<boost::thread>::iterator it;
        for (it = threads_.begin(); it != threads_.end(); ++it) {
            it->interrupt();
            it->join();
        }
    }

  } /* namespace gdtp */
} /* namespace gr */

