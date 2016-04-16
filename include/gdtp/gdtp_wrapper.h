/* -*- c++ -*- */
/*
 * Copyright 2015 <+YOU OR YOUR COMPANY+>.
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


#ifndef INCLUDED_GDTP_GDTP_WRAPPER_H
#define INCLUDED_GDTP_GDTP_WRAPPER_H

#include <gdtp/api.h>
#include <gnuradio/block.h>

namespace gr {
  namespace gdtp {

    /*!
     * \brief <+description of block+>
     * \ingroup gdtp
     *
     */
    class GDTP_API gdtp_wrapper : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<gdtp_wrapper> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of gdtp::gdtp_wrapper.
       *
       * To avoid accidental use of raw pointers, gdtp::gdtp_wrapper's
       * constructor is in a private implementation
       * class. gdtp::gdtp_wrapper::make is the public interface for
       * creating new instances.
       */
      static sptr make(bool debug, uint64_t src_addr, uint64_t dest_addr,  const std::vector<int> &reliable, std::string addr_mode, std::string addr_src, int ack_timeout, int max_retry, int max_seq_no, std::string scheduler, int num_flows=1);
    };

  } // namespace gdtp
} // namespace gr

#endif /* INCLUDED_GDTP_GDTP_WRAPPER_H */

