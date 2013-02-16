//
// Copyright (C) 2013 Eitan Zahavi Technion EE Department
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

package inet.examples.inet.dcn_fat_tree;

import inet.networklayer.autorouting.ipv4.IPv4NetworkConfigurator;
import inet.nodes.ethernet.Eth40G;
import inet.nodes.ethernet.EtherSwitch;
import inet.nodes.inet.Router;
import inet.nodes.inet.StandardHost;


network dcn_xgft_2l
{
    parameters:
       int m1 = default(4);
       int m2 = default(8);
       int w1 = 1;
       int w2 = default(4);

    submodules:
        switch_l1[m2]: EtherSwitch {
            @display("p=50,200,row,200");
            gates: ethg[m1+w2];
        }
        switch_l2[w2]: EtherSwitch {
            @display("p=50,0,row,200");
            gates: ethg[m2];
        }
        host[m1*m2]: StandardHost {
            @display("p=50,300,row,50");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=37,31");
            config = xmldoc("fat_tree.xml");
        }
        
    connections:
        for s=0..w2-1, for l=0..m2-1 {
            switch_l2[s].ethg[l] <--> Eth40G <--> switch_l1[l].ethg[m1+s];
        }
        for l=0..m2-1, for h=0..m1-1 { 
            switch_l1[l].ethg[h] <--> Eth40G <--> host[h+l*m1].ethg++;
        }
}


network dcn_xgft_3l
{
    parameters:
       int m1 = default(8);
       int m2 = default(4);
       int m3 = default(4);
       int w1 = 1;
       int w2 = default(4);
	   int w3 = default(4);
    submodules:
        switch_l1[m3*m2]: EtherSwitch {
            @display("is=s;p=50,400,r,200");
            gates: ethg[m1+w2];
        }
        switch_l2[w2*m3]: EtherSwitch {
            @display("is=s;p=50,300,r,200");
            gates: ethg[m2+w3];
        }
        switch_l3[w2*w3]: EtherSwitch {
            @display("is=s;p=50,100,r,200");
            gates: ethg[m3];
        }
        host[m1*m2*m3]: StandardHost {
 //           @display("is=s;p=50,500,r,50");
 			@display("is=s");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=37,31");
            config = xmldoc("fat_tree.xml");
        }
        
    connections:
        // first connect all 2 level trees m3 of them
        for t=0..m3-1, for s=0..w2-1, for l=0..m2-1 {
             switch_l2[s+t*w2].ethg[l] <--> Eth40G <--> switch_l1[l+t*m2].ethg[m1+s];
        }
        for t=0..m3-1, for l=0..m2-1, for h=0..m1-1 { 
             switch_l1[l+t*m2].ethg[h] <--> Eth40G <--> host[h+l*m1+t*m2*m1].ethg++;
        }
        
        // spines to top of the 2 level trees
        for t=0..m3-1, for r=0..w3-1, for s=0..w2-1 {
            switch_l2[s+t*w2].ethg[r+m2] <--> Eth40G <--> switch_l3[r+s*w3].ethg[t];
        }
}
