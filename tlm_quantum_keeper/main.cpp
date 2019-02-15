/*
 * Copyright 2017 Matthias Jung
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *     - Matthias Jung
 */

#include <iostream>
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#define USEQK
//#define LONG_RUN
#define PRINTING

using namespace std;

class exampleInitiator: sc_module, tlm::tlm_bw_transport_if<>
{
    private:
#ifdef USEQK
    tlm_utils::tlm_quantumkeeper quantumKeeper;
#endif

    public:
    tlm::tlm_initiator_socket<> iSocket;
    SC_CTOR(exampleInitiator) : iSocket("iSocket")
    {
        iSocket.bind(*this);
        SC_THREAD(process);
#ifdef USEQK
        quantumKeeper.set_global_quantum(sc_time(10000,SC_NS)); // STATIC!
        quantumKeeper.reset();
#endif
    }

    void process()
    {
        // Write to memory:
#ifdef LONG_RUN
        for (int j = 0; j < 100000; j++)
#endif
        for (int i = 0; i < 1024; i++) {
            tlm::tlm_generic_payload trans;
            unsigned char data = rand();
            trans.set_address(i);
            trans.set_data_length(1);
            trans.set_streaming_width(1);
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_data_ptr(&data);
            trans.set_response_status( tlm::TLM_INCOMPLETE_RESPONSE );
#ifdef USEQK
            sc_time delay = quantumKeeper.get_local_time();
#else
            sc_time delay = SC_ZERO_TIME;
#endif

#ifdef PRINTING
            cout << this->name() << ": B_TRANSPORT @" << sc_time_stamp()
                 << " Local Time " << quantumKeeper.get_local_time() << endl;
#endif
            iSocket->b_transport(trans, delay);
            if ( trans.is_response_error() )
              SC_REPORT_FATAL(name(), "Response error from b_transport");

#ifdef USEQK
            quantumKeeper.set(delay); // Anotate the time of the target
            quantumKeeper.inc(sc_time(10,SC_NS)); // Consume computation time
#else
            wait(delay+sc_time(10,SC_NS));
#endif

#ifdef USEQK
            if(quantumKeeper.need_sync())
            {
#ifdef PRINTING
                cout << this->name() << ": Context Switch @"
                     << sc_time_stamp() << endl;
#endif
                quantumKeeper.sync();
            }
#endif
        }
    }

    public:
    // Dummy method:
    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range,
                                   sc_dt::uint64 end_range)
    {
        SC_REPORT_FATAL(this->name(),"invalidate_direct_mem_ptr not implement");
    }

    // Dummy method:
    tlm::tlm_sync_enum nb_transport_bw(
            tlm::tlm_generic_payload& trans,
            tlm::tlm_phase& phase,
            sc_time& delay)
    {
        SC_REPORT_FATAL(this->name(),"nb_transport_bw is not implemented");
        return tlm::TLM_ACCEPTED;
    }
};

class exampleTarget : sc_module, tlm::tlm_fw_transport_if<>
{
    private:
    unsigned char mem[1024];

    public:
    tlm::tlm_target_socket<> tSocket1;
    tlm::tlm_target_socket<> tSocket2;

    SC_CTOR(exampleTarget) : tSocket1("tSocket1"), tSocket2("tSocket2")
    {
        tSocket1.bind(*this);
        tSocket2.bind(*this);
    }

    void b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
    {
        if (trans.get_address() >= 1024)
        {
             trans.set_response_status( tlm::TLM_ADDRESS_ERROR_RESPONSE );
             return;
        }

        if (trans.get_data_length() != 1)
        {
             trans.set_response_status( tlm::TLM_BURST_ERROR_RESPONSE );
             return;
        }

        if(trans.get_command() == tlm::TLM_WRITE_COMMAND)
        {
            memcpy(&mem[trans.get_address()], // destination
                   trans.get_data_ptr(),      // source
                   trans.get_data_length());  // size
        }
        else // (trans.get_command() == tlm::TLM_READ_COMMAND)
        {
            memcpy(trans.get_data_ptr(),      // destination
                   &mem[trans.get_address()], // source
                   trans.get_data_length());  // size
        }

        delay = delay + sc_time(33, SC_NS);

        trans.set_response_status( tlm::TLM_OK_RESPONSE );
    }

    // Dummy method
    virtual tlm::tlm_sync_enum nb_transport_fw(
            tlm::tlm_generic_payload& trans,
            tlm::tlm_phase& phase,
            sc_time& delay )
    {
        SC_REPORT_FATAL(this->name(),"nb_transport_fw is not implemented");
        return tlm::TLM_ACCEPTED;
    }

    // Dummy method
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                            tlm::tlm_dmi& dmi_data)
    {
        SC_REPORT_FATAL(this->name(),"get_direct_mem_ptr is not implemented");
        return false;
    }

    // Dummy method
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        SC_REPORT_FATAL(this->name(),"transport_dbg is not implemented");
        return 0;
    }

};

int sc_main (int __attribute__((unused)) sc_argc,
             char __attribute__((unused)) *sc_argv[])
{

    exampleInitiator * cpu1 = new exampleInitiator("cpu1");
    exampleInitiator * cpu2 = new exampleInitiator("cpu2");
    exampleTarget * memory  = new exampleTarget("memory");

    cpu1->iSocket.bind(memory->tSocket1);
    cpu2->iSocket.bind(memory->tSocket2);

    sc_start();

    return 0;
}
