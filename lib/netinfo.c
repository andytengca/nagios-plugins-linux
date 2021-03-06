/*
 * License: GPLv3+
 * Copyright (c) 2014 Davide Madrisan <davide.madrisan@gmail.com>
 *
 * A library for getting some network interfaces.statistics.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#ifdef HAVE_LINUX_IF_LINK_H
# include <linux/if_link.h>
#else
# include <linux/rtnetlink.h>
#endif

#include "common.h"
#include "logging.h"
#include "string-macros.h"
#include "messages.h"
#include "netinfo.h"
#include "xalloc.h"

static struct iflist* get_netinfo_snapshot (void)
{
  int family;
  struct ifaddrs *ifaddr, *ifa;

  if (getifaddrs (&ifaddr) == -1)
    plugin_error (STATE_UNKNOWN, errno, "getifaddrs() failed");

  struct iflist *iflhead = NULL, *iflprev = NULL, *ifl;

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
      if (ifa->ifa_addr == NULL || ifa->ifa_data == NULL)
        continue;

      family = ifa->ifa_addr->sa_family;
      if (family != AF_PACKET)
	continue;

      struct rtnl_link_stats *stats = ifa->ifa_data;
      ifl = xmalloc (sizeof (struct iflist));

      ifl->ifname = xstrdup (ifa->ifa_name);
      ifl->tx_packets = stats->tx_packets;
      ifl->rx_packets = stats->rx_packets;
      ifl->tx_bytes   = stats->tx_bytes; 
      ifl->rx_bytes   = stats->rx_bytes;
      ifl->tx_errors  = stats->tx_errors;
      ifl->rx_errors  = stats->rx_errors;
      ifl->tx_dropped = stats->tx_dropped;
      ifl->rx_dropped = stats->rx_dropped;
      ifl->multicast  = stats->multicast;
      ifl->collisions = stats->collisions;
      ifl->next = NULL;

      if (iflhead == NULL)
	iflhead = ifl;
      else
	iflprev->next = ifl;
      iflprev = ifl;
    }

  freeifaddrs (ifaddr);
  return iflhead;
}

struct iflist* netinfo (unsigned int seconds)
{
  struct iflist *iflhead = get_netinfo_snapshot ();

  if (seconds > 0)
   {
      sleep (seconds);
      struct iflist *ifl, *ifl2, *iflhead2 = get_netinfo_snapshot ();

      for (ifl = iflhead, ifl2 = iflhead2; ifl != NULL && ifl2 != NULL;
	   ifl = ifl->next, ifl2 = ifl2->next)
	{
	  dbg ("network interface '%s'\n", ifl->ifname);
	  if (STRNEQ (ifl->ifname, ifl2->ifname))
	    plugin_error (STATE_UNKNOWN, 0,
			  "bug in netinfo(), please contact the developers");

	  dbg ("\ttx_packets : %u %u\n", ifl->tx_packets, ifl2->tx_packets);
	  ifl->tx_packets = (ifl2->tx_packets - ifl->tx_packets) / seconds;

	  dbg ("\trx_packets : %u %u\n", ifl->rx_packets, ifl2->rx_packets);
	  ifl->rx_packets = (ifl2->rx_packets - ifl->rx_packets) / seconds;

	  dbg ("\ttx_bytes   : %u %u\n", ifl->tx_bytes, ifl2->tx_bytes);
	  ifl->tx_bytes   = (ifl2->tx_bytes   - ifl->tx_bytes  ) / seconds;

	  dbg ("\trx_bytes   : %u %u\n", ifl->rx_bytes, ifl2->rx_bytes);
	  ifl->rx_bytes   = (ifl2->rx_bytes   - ifl->rx_bytes  ) / seconds;

	  dbg ("\ttx_errors  : %u %u\n", ifl->tx_errors, ifl2->tx_errors);
	  ifl->tx_errors  = (ifl2->tx_errors  - ifl->tx_errors ) / seconds;

	  dbg ("\trx_errors  : %u %u\n", ifl->rx_errors, ifl2->rx_errors);
	  ifl->rx_errors  = (ifl2->rx_errors  - ifl->rx_errors ) / seconds;

	  ifl->tx_dropped = (ifl2->tx_dropped - ifl->tx_dropped) / seconds;
	  dbg ("\ttx_dropped : %u %u\n", ifl->tx_dropped, ifl2->tx_dropped);

	  dbg ("\trx_dropped : %u %u\n", ifl->rx_dropped, ifl2->rx_dropped);
	  ifl->rx_dropped = (ifl2->rx_dropped - ifl->rx_dropped) / seconds;

	  dbg ("\tmulticast  : %u %u\n", ifl->multicast, ifl2->multicast);
	  ifl->multicast  = (ifl2->multicast  - ifl->multicast ) / seconds;

	  dbg ("\tcollisions : %u %u\n", ifl->collisions, ifl2->collisions);
	  ifl->collisions = (ifl2->collisions - ifl->collisions) / seconds;
	}
      freeiflist (iflhead2);
   }

  return iflhead;
}

void freeiflist (struct iflist *iflhead)
{
  struct iflist *ifl = iflhead, *iflnext;

  while (ifl != NULL)
    {
      iflnext = ifl->next;
      free (ifl->ifname);
      free (ifl);
      ifl = iflnext;
    }
}
