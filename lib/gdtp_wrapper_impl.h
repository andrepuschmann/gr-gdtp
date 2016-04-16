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

#ifndef INCLUDED_GDTP_GDTP_WRAPPER_IMPL_H
#define INCLUDED_GDTP_GDTP_WRAPPER_IMPL_H

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <gdtp/gdtp_wrapper.h>
#include "libgdtp.h"

using namespace libgdtp;

namespace gr {
namespace gdtp {

typedef std::map<std::string, FlowId> PortMap;

class gdtp_wrapper_impl : public gdtp_wrapper
{
private:
    // block parameters
    bool debug_;
    Addr src_address_;
    Addr dest_address_;
    const std::vector<int> &reliable_;
    std::string addr_mode_;
    std::string addr_src_;
    int ack_timeout_;
    int max_retry_;
    int max_seq_no_;
    std::string scheduler_;
    int num_flows;

    // local block members
    std::unique_ptr<libgdtp::Gdtp> gdtp_;
    PortMap gdtpPorts_;
    pmt::pmt_t mac_in, mac_out;
    boost::ptr_vector<boost::thread> threads_;

    // block methods
    void tx_handler();
    void flowout_handler(std::string outport_name, FlowId id);

public:
    gdtp_wrapper_impl(bool debug, uint64_t src_addr, uint64_t dest_addr, const std::vector<int> &reliable, std::string addr_mode, std::string addr_src, int ack_timeout, int max_retry, int max_seq_no, std::string scheduler, int num_flows);
    ~gdtp_wrapper_impl();

    // Where all the action really happens
    void register_flows(int num_flows, const char *inport_base, const char *outport_base);
    void pack(std::string portName, FlowId id, pmt::pmt_t msg);
    void unpack(std::string inport_name, pmt::pmt_t msg);
};

} // namespace gdtp
} // namespace gr

#endif /* INCLUDED_GDTP_GDTP_WRAPPER_IMPL_H */
