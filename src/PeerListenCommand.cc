/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "PeerListenCommand.h"
#include "PeerInteractionCommand.h"

PeerListenCommand::PeerListenCommand(int cuid,
				     TorrentDownloadEngine* e,
				     const BtContextHandle& btContext)
  :BtContextAwareCommand(cuid, btContext),
   e(e) {}

PeerListenCommand::~PeerListenCommand() {}

int PeerListenCommand::bindPort(int portRangeStart, int portRangeEnd) {
  if(portRangeStart > portRangeEnd) {
    return -1;
  }
  for(int port = portRangeStart; port <= portRangeEnd; port++) {
    try {
      socket->beginListen(port);
      logger->info("CUID#%d - using port %d for accepting new connections",
		   cuid, port);
      return port;
    } catch(RecoverableException* ex) {
      logger->error("CUID#%d - an error occurred while binding port=%d",
		    ex, cuid, port);
      socket->closeConnection();
      delete ex;
    }
  }
  return -1;
}

bool PeerListenCommand::execute() {
  if(btRuntime->isHalt()) {
    return true;
  }
  for(int i = 0; i < 3 && socket->isReadable(0); i++) {
    SocketHandle peerSocket;
    try {
      peerSocket = socket->acceptConnection();
      pair<string, int> peerInfo;
      peerSocket->getPeerInfo(peerInfo);
      pair<string, int> localInfo;
      peerSocket->getAddrInfo(localInfo);
      if(peerInfo.first != localInfo.first &&
	 btRuntime->getConnections() < MAX_PEERS) {
	PeerHandle peer = PeerHandle(new Peer(peerInfo.first, peerInfo.second,
					      btContext->getPieceLength(),
					      btContext->getTotalLength()));
	if(peerStorage->addIncomingPeer(peer)) {
	  int newCuid =  btRuntime->getNewCuid();
	  peer->cuid = newCuid;
	  PeerInteractionCommand* command =
	    new PeerInteractionCommand(newCuid, peer, e,
				       btContext,
				       peerSocket,
				       PeerInteractionCommand::RECEIVER_WAIT_HANDSHAKE);
	  e->commands.push_back(command);
	  logger->debug("CUID#%d - incoming connection, adding new command CUID#%d", cuid, newCuid);
	}
      }
    } catch(RecoverableException* ex) {
      logger->error("CUID#%d - error in accepting connection", ex, cuid);
      delete ex;
    }		    
  }
  e->commands.push_back(this);
  return false;
}
