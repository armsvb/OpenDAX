/*  opendcs - An open source distributed control system
 *  Copyright (c) 2007 Phil Birkelbach
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *

 * This file contains the messaging code for the library
 */
 
#include <common.h>
#include <dcs/tagbase.h>
#include <opendcs.h>
#include <dcs/message.h>
#include <sys/ipc.h>
#include <sys/msg.h>

static int __msqid;

/* Connects to the message queue and sends a registration to the core */
static int mod_reg(char *name) {
    dcs_message outmsg;
    int result;
    
    __msqid=msgget(DCS_IPC_KEY,0660);
    if(__msqid < 0) {
        perror("msgget");
        return -1;
    }
    
    outmsg.handle=1;
    outmsg.command=MSG_MOD_REG;
    outmsg.pid=getpid();
    if(name) {
        
    } else {
        outmsg.size=0;
    }
    result=msgsnd(__msqid,(struct msgbuff *)(&outmsg),MSG_SIZE + sizeof(char),0);
    if(result) {
        xerror("dcs_mod_register: %s",strerror(errno));
        return -1;
    }
}

int dcs_mod_register(char *name) {
    return mod_reg(name);
}

int dcs_mod_unregister(void) {
    return mod_reg(NULL);
}