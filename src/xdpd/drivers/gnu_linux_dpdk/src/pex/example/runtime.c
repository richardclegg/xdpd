/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
* @file runtime.c
* @author Ivano Cerrato<ivano.cerrato (at) polito.it>
*
* @brief Executes the PEX.
*/

#include <assert.h>
#include <rte_mbuf.h>

#include "main.h"

int do_pex(void *useless)
{
	mbuf_array_t pkts_received, pkts_to_send;
	
	//FIXME: the next two lines are just for the compiler
	if(useless != NULL)
		return -1;
	
	fprintf(stdout,"%s is started!\n", module_name);
	fflush(stdout);

	while(1)
	{
		int sval;
		sem_getvalue(pex_params.semaphore, &sval);
	
		sem_wait(pex_params.semaphore);

		/*1) Receive incoming packets */
		
        pkts_received.n_mbufs = rte_ring_sc_dequeue_burst(pex_params.to_pex_queue,(void **)&pkts_received.array[0],PKT_TO_PEX_THRESHOLD);

		if(likely(pkts_received.n_mbufs > 0))
		{
			fprintf(stdout,"%s Received %d pkts from xDPD!\n", module_name, pkts_received.n_mbufs);
		
			pkts_to_send.n_mbufs = 0;
			int i;
			for (i=0;i < pkts_received.n_mbufs;i++)
			{
				/*2) Operate on the packet */
			
				//TODO: write here the logic of the NF
			
				pkts_to_send.array[pkts_to_send.n_mbufs] = pkts_received.array[i];
		    	pkts_to_send.n_mbufs++;
		    	
				/*
				*	This check is required because the PEX could create (and then send) new packets, which may cause
				*	the reaching of the threshold before that all the packets just received are proecessed
				*/
			    if (unlikely((pkts_to_send.n_mbufs == PKT_TO_PEX_THRESHOLD)))
			    {
			    	/*3) Send the processed packet */
			    
			    	fprintf(stderr,"%s Threshold reached! Send %d packets to xDPD.\n",module_name,PKT_TO_PEX_THRESHOLD);
			    
			        int ret = rte_ring_sp_enqueue_burst(pex_params.to_xdpd_queue,(void *const*)pkts_to_send.array,(unsigned)pkts_to_send.n_mbufs);
			        
			        if (unlikely(ret < pkts_to_send.n_mbufs)) 
			        {
			        	fprintf(stderr,"%s Not enough room in the ring towards xDPD to enqueue; the packet will be dropped.\n", module_name);
						do {
							struct rte_mbuf *pkt_to_free = pkts_to_send.array[ret];
							rte_pktmbuf_free(pkt_to_free);
						} while (++ret < pkts_to_send.n_mbufs);
					}
			        
		    		pkts_to_send.n_mbufs = 0;
		    	}	
			}
			
			/*3) Send the processed packet */
			if(likely(pkts_to_send.n_mbufs > 0))
			{			
				int ret = rte_ring_sp_enqueue_burst(pex_params.to_xdpd_queue,(void *const*)pkts_to_send.array,(unsigned)pkts_to_send.n_mbufs);
			        
	        	if (unlikely(ret < pkts_to_send.n_mbufs)) 
		        {
		        	fprintf(stderr,"%s Not enough room in the ring towards xDPD to enqueue; the packet will be dropped.\n", module_name);
					do {
						struct rte_mbuf *pkt_to_free = pkts_to_send.array[ret];
						rte_pktmbuf_free(pkt_to_free);
					} while (++ret < pkts_to_send.n_mbufs);
				}
			
			}

		}/* End of if(pkts_received.n_mbufs) */
		else
		{
			fprintf(stderr,"%s The PEX has been woken up without packets to be processed!\n",module_name);
			assert(0);
		}

	}/*End of while true*/
	return 0;
}
