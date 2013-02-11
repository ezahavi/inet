//
// Copyright (C) 2012 Opensim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

package inet.examples.inet.dcn_1s_4n;

import inet.networklayer.autorouting.ipv4.IPv4NetworkConfigurator;
import inet.nodes.ethernet.Eth40G;
import inet.nodes.ethernet.EtherSwitch;
import inet.nodes.inet.Router;
import inet.nodes.inet.StandardHost;


network DCN_1S_4N
{
    submodules:
        switch: EtherSwitch {
            @display("is=s");
        }
        host[4]: StandardHost {
            @display("is=s");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=37,31");
            config = xmldoc("Dcn_1S_4N.xml");
        }
        
    connections:
        for i=0..3 {
            host[i].ethg++ <--> Eth40G <--> switch.ethg++;
        }
}
