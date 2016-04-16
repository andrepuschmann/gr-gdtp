/*
 * Copyright 2015, Andre Puschmann <andre.puschmann@tu-ilmenau.de>
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <gnuradio/top_block.h>
#include <gnuradio/blocks/tuntap_pdu.h>
#include <gnuradio/blocks/message_debug.h>
#include "gdtp/gdtp_wrapper.h"



using namespace gr;

int main(int argc, char **argv)
{
    bool debug = true;
    const std::vector<int> reliable(1, 1);
    std::string addr_mode("explicit");
    std::string addr_src("tun0");
    int ack_timeout = 100;
    int max_retry = 7;
    int max_seq_no = 127;
    std::string scheduler("fifo");

    // Construct a top block that will contain flowgraph blocks.  Alternatively,
    // one may create a derived class from top_block and hold instantiated blocks
    // as member data for later manipulation.
    top_block_sptr tb = make_top_block("tun_trx_test");

    // create the blocks
    blocks::tuntap_pdu::sptr tun = blocks::tuntap_pdu::make(addr_src, 1000, true);
    gdtp::gdtp_wrapper::sptr tx = gdtp::gdtp_wrapper::make(debug, 1, 2, reliable, addr_mode, addr_src, ack_timeout, max_retry, max_seq_no, scheduler);
    gdtp::gdtp_wrapper::sptr rx = gdtp::gdtp_wrapper::make(debug, 2, 1, reliable, addr_mode, addr_src, ack_timeout, max_retry, max_seq_no, scheduler);
    blocks::message_debug::sptr debug_sink = blocks::message_debug::make();

    // connect the blocks together
    tb->msg_connect(tun,"pdus", tx, "flowin");
    tb->msg_connect(tx,"toMAC", rx, "fromMAC");
    tb->msg_connect(rx,"flowout", debug_sink, "print");

    // Tell GNU Radio runtime to start flowgraph threads; the foreground thread
    // will block until either flowgraph exits (this example doesn't) or the
    // application receives SIGINT (e.g., user hits CTRL-C).
    //
    // Real applications may use tb->start() which returns, allowing the foreground
    // thread to proceed, then later use tb->stop(), followed by tb->wait(), to cleanup
    // GNU Radio before exiting.
    tb->run();

    // Exit normally.
    return 0;
}

