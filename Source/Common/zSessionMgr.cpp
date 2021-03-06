#include "SrvEngine.h"

zSession::zSession(NetSocket* _socket) :socket(_socket),webSocket(NULL), nSessID(0)
{
	initEncrypt();
}

zSession::zSession(CWebClient* _webSocket,int i):socket(NULL), webSocket(_webSocket), nSessID(0)
{
	initEncrypt();
}

zSession::~zSession()
{

}

void zSession::sendMsg(const void* msg, int32_t size)
{
	if (socket)
	{
		socket->ParkMsg((const uint8_t*)msg, size);
	}
	if (webSocket)
	{
		webSocket->sendresponse((const PbMsgWebSS*)msg, size);
	}
}

void zSession::sendMsgProto(uint16_t cmd, uint16_t cmdType, const ::google::protobuf::Message& proto)
{
	sendMsgProto(cmd,cmdType,0,0,proto);
}

void zSession::sendMsgProto(uint16_t cmd, uint16_t cmdType, uint64_t clientSessID, const ::google::protobuf::Message& proto)
{
	sendMsgProto(cmd, cmdType, clientSessID, 0, proto);
}

void zSession::sendMsgProto(uint16_t cmd, uint16_t cmdType, uint64_t clientSessID,uint32_t fepServerID, const ::google::protobuf::Message& proto)
{
	BUFFER_CMD(PbMsgWebSS, send, MAX_USERDATASIZE);
	send->cmd = cmd;
	send->cmdType = cmdType;
	send->clientSessID = clientSessID;
	send->fepServerID = fepServerID;
	send->size = proto.ByteSize();
	proto.SerializeToArray(send->data, send->size);
	sendMsg(send, sizeof(PbMsgWebSS) + send->size);
}

void zSession::exist()
{
	if (socket)
	{
		socket->OnEventColse();
	}
}

zSessionMgr::zSessionMgr():mServerObj(true,1), mClientObj(true, 4), mSessionObj(true, 64)
{
}

zSessionMgr::~zSessionMgr()
{
}

zSession* zSessionMgr::getByServerID(uint32_t nServerID)
{
	struct FindSession : public execEntry<zSession>
	{
		FindSession(uint32_t _nServerID):nServerID(_nServerID),_session(NULL)
		{
		}
		virtual bool exec(zSession* e)
		{
			if (_session == NULL && e->GetRemoteServerID() == nServerID)
			{
				_session = e;
			}
			return true;
		}
		uint32_t nServerID;
		zSession* _session;
	};
	FindSession find(nServerID);
	execEveryEntry(find);
	return find._session;
}

zSession* zSessionMgr::getWs(uint32_t nServerID)
{
	return getByServerID(nServerID);
}

zSession* zSessionMgr::getLs(uint32_t nServerID)
{
	return getByServerID(nServerID);
}

zSession* zSessionMgr::getFep(uint32_t nServerID)
{
	return getByServerID(nServerID);
}

zSession* zSessionMgr::getSs(uint32_t nServerID)
{
	return getByServerID(nServerID);
}

zSession* zSessionMgr::getDp(uint32_t nServerID)
{
	return getByServerID(nServerID);
}

void zSessionMgr::getSsList(std::vector<zSession*>& outSessions)
{
	struct FindSession : public execEntry<zSession>
	{
		FindSession(uint32_t _nServerType) :nServerType(_nServerType)
		{
		}
		virtual bool exec(zSession* e)
		{
			if (e->GetRemoteServerType() == nServerType)
			{
				vecSessions.push_back(e);
			}
			return true;
		}
		uint32_t nServerType;
		std::vector<zSession*> vecSessions;
	};
	FindSession find(::config::server_t_scene);
	execEveryEntry(find);
	std::vector<zSession*>::iterator it = find.vecSessions.begin();
	for (; it != find.vecSessions.end(); ++it)
	{
		outSessions.push_back(*it);
	}
}

bool zSessionMgr::sendMsg(int64_t sessid,const NetMsgSS* pMsg, int32_t nSize)
{
	zSession* session = get(sessid);
	if (session)
	{
		session->sendMsg(pMsg, nSize);
		return true;
	}
	return false;
}

bool zSessionMgr::sendToWs(const NetMsgSS* pMsg, int32_t nSize)
{
	zSession* ws = getWs();
	if (ws)
	{
		ws->sendMsg(pMsg, nSize);
		return true;
	}
	return false;
}

bool zSessionMgr::sendToDp(const NetMsgSS* pMsg, int32_t nSize)
{
	zSession* dps = getDp();
	if (dps)
	{
		dps->sendMsg(pMsg, nSize);
		return true;
	}
	return false;
}

bool zSessionMgr::sendToLs(const NetMsgSS* pMsg, int32_t nSize)
{
	zSession* ls = getLs();
	if (ls)
	{
		ls->sendMsg(pMsg, nSize);
		return true;
	}
	return false;
}

bool zSessionMgr::sendToSs(int32_t serverid, const NetMsgSS* pMsg, int32_t nSize)
{
	zSession* ss = getSs(serverid);
	if (ss)
	{
		ss->sendMsg(pMsg, nSize);
		return true;
	}
	return false;
}

bool zSessionMgr::sendToFep(int32_t serverid, const NetMsgSS* pMsg, int32_t nSize)
{
	zSession* fep = getFep(serverid);
	if (fep)
	{
		fep->sendMsg(pMsg, nSize);
		return true;
	}
	return false;
}

bool zSessionMgr::bind(zNetSerivce* zNetSrv, const ::config::SerivceInfo& info,NetMsgEnter fEnter, NetMsgOn fMsg, NetMsgExit fExit)
{
	zServerMgr* pSrvMgr = zNetSrv->SrvSerivceMgr()->GetServerMgr(zNetSrv->GetServerID());
	if (!pSrvMgr)
	{
		H::logger->error("Not found serverID=%u serivces.xml", zNetSrv->GetServerID());
		return false;
	}

	if (info.maxconntions() < 1)
	{
		H::logger->error("maxconntions < 1");
		ASSERT(0);
		return false;
	}

	NetServer* pServer = mServerObj.construct(info.maxconntions());
	std::vector<NetSocket*>& allSockets = pServer->GetNetSockets();
	for (size_t i = 0; i < allSockets.size(); ++i)
	{
		zSession* pSession = mSessionObj.construct(allSockets[i]);
		pSession->nSessID = allSockets[i]->SocketID();
		pSession->SetDataProto(pSrvMgr->GetDataProto());
		if (!add(pSession))
		{
			mSessionObj.destroy(pSession);
			ASSERT(0);
			continue;
		}
	}

	pServer->SetTimeout(SERVER_TIMEOUT);
	pServer->SetAddress(info.serivceip().c_str(),info.serivceport());
	pServer->SetHandler(fEnter, fMsg, fExit);
	pServer->Start();

	serverListAdd.push_back(pServer);

	H::logger->info("启动一个服务:ID=%d,IP=%s,PORT=%d", info.serivceid(), info.serivceip().c_str(),info.serivceport());

	return true;

}

zSession* zSessionMgr::connect(zNetSerivce* zNetSrv, const ::config::SerivceInfo& info, NetMsgEnter fEnter, NetMsgOn fMsg, NetMsgExit fExit)
{
	zServerMgr* pSrvMgr = zNetSrv->SrvSerivceMgr()->GetServerMgr(zNetSrv->GetServerID());
	if (!pSrvMgr)
	{
		H::logger->error("Not found serverID=%u serivces.xml", zNetSrv->GetServerID());
		return NULL;
	}

	NetClient* pClient = mClientObj.construct();
	zSession* pSession = mSessionObj.construct(pClient->GetSocket());
	pSession->nSessID = pClient->GetSocket()->SocketID();
	if (!add(pSession))
	{	
		mSessionObj.destroy(pSession);
		ASSERT(0);
		return NULL;
	}

	pSession->SetDataProto(pSrvMgr->GetDataProto());

	pClient->SetAddress(info.serivceip().c_str(), info.serivceport());
	pClient->SetHandler(fEnter, fMsg, fExit);
	pClient->Start();

	clientListAdd.push_back(pClient);

	H::logger->info("启动一个连接:ID=%d,IP=%s,PORT=%d", info.serivceid(), info.serivceip().c_str(), info.serivceport());

	return pSession;

}

zSession* zSessionMgr::add(uint64_t _sessID, NetSocket* _socket)
{
	zSession* pSession = mSessionObj.construct(_socket);
	pSession->SetID(_sessID);
	if (!add(pSession))
	{
		mSessionObj.destroy(pSession);
		return NULL;
	}
	return pSession;
}

zSession* zSessionMgr::addWeb(uint64_t _sessID, CWebClient* _socket)
{
	zSession* pSession = mSessionObj.construct(_socket,0);
	pSession->SetID(_sessID);
	if (!add(pSession))
	{
		mSessionObj.destroy(pSession);
		return NULL;
	}
	return pSession;
}

void zSessionMgr::updateIO(const zTaskTimer* timer)
{
	updateServerIO(timer);
	updateClientIO(timer);
}

void zSessionMgr::updateServerIO(const zTaskTimer* timer)
{
	if (!serverListAdd.empty())
	{
		for (std::vector<NetServer*>::iterator itSrv = serverListAdd.begin(); itSrv != serverListAdd.end(); ++itSrv)
		{
			serverListUsed.push_back(*itSrv);
		}
		serverListAdd.clear();
	}

	for (std::vector<NetServer*>::iterator itSrv = serverListUsed.begin(); itSrv != serverListUsed.end(); ++itSrv)
	{
		(*itSrv)->OnUpdate();
	}
}

void zSessionMgr::updateClientIO(const zTaskTimer* timer)
{
	if (!clientListAdd.empty())
	{
		for (std::vector<NetClient*>::iterator itCli = clientListAdd.begin(); itCli != clientListAdd.end(); ++itCli)
		{
			clientListUsed.push_back(*itCli);
		}
		clientListAdd.clear();
	}

	for (std::vector<NetClient*>::iterator itCli = clientListUsed.begin(); itCli != clientListUsed.end(); ++itCli)
	{
		(*itCli)->OnUpdate();
	}
}

NetServer* zSessionMgr::GetNetServer(int32_t serverID)
{	
	for (std::vector<NetServer*>::iterator it = serverListAdd.begin(); it != serverListAdd.end(); ++it)
	{
		if ((*it)->SocketID() == serverID)
		{
			return *it;
		}
	}
	for (std::vector<NetServer*>::iterator it = serverListUsed.begin(); it != serverListUsed.end(); ++it)
	{
		if ((*it)->SocketID() == serverID)
		{
			return *it;
		}
	}
	return NULL;
}

NetClient* zSessionMgr::GetNetClient(int32_t clientID)
{
	for (std::vector<NetClient*>::iterator it = clientListAdd.begin(); it != clientListAdd.end(); ++it)
	{
		if ((*it)->CID() == clientID)
		{
			return *it;
		}
	}
	for (std::vector<NetClient*>::iterator it = clientListUsed.begin(); it != clientListUsed.end(); ++it)
	{
		if ((*it)->CID() == clientID)
		{
			return *it;
		}
	}
	return NULL;
}
