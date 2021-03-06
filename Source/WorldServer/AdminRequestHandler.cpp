#include "AdminRequestHandler.h"
#include "MyHttpServer.h"

#include "WorldUser.h"
#include "WorldUserMgr.h"
#include "GameService.h"
#include "OfflineUser.h"
#include "OfflineUserMgr.h"
#include "SceneRoom.h"
#include "SceneRoomMgr.h"

#include "Poco/JSON/Parser.h"
#include "Poco/JSON/ParseHandler.h"
#include "Poco/JSON/JSONException.h"
#include "Poco/StreamCopier.h"
#include "Poco/Dynamic/Var.h"
#include "Poco/JSON/Query.h"
#include "Poco/JSON/PrintHandler.h"

using namespace Poco::Dynamic;
using namespace Poco;

AdminRequestHandler::AdminRequestHandler(MyHttpServer* httpServer, RequestType queryType):_httpServer(httpServer), _queryType(queryType)
{

}

void AdminRequestHandler::handleRequest(HTTPServerRequest &req, HTTPServerResponse &resp)
{	


	std::map<std::string, std::string> params;
	_httpServer->getRequestArgs(req,params);

	int status = _httpServer->checkAuth(params);
	if (status != 0 )
	{
		resp.setStatus(HTTPResponse::HTTP_OK);
		resp.setContentType("text/html");
		std::ostream& out = resp.send();
		out << "{\"status\": " << status 
			<< ",\"info\":\"Auth Fail!\"}";
		return;
	}

	switch (_queryType)
	{
	case eSendRoomCard:DoSendRoomCard(req, resp);break;
	case eSendMail:DoSendMail(req, resp);break;
	case eSendNotice:DoSendNotice(req, resp);break;
	case eIosCharge:DoIosCharge(req, resp);break;
	case eRoomInfoList:DoRoomInfo(req, resp);break;
	case eMyRoom:DoMyRoom(req, resp);	break;
	case eUserEdit:DoUserEdit(req, resp);break;
	case eExitGame:DoExitGame(req, resp);break;
	case eReloadConfig:
		reload_config_product_id = _httpServer->GetKeyValueInt32("product_id",params);
		DoReloadConfig(req, resp);
		break;
	case eSceneList:DoSceneList(req, resp); break;
	default:
		resp.setStatus(HTTPResponse::HTTP_OK);
		resp.setContentType("text/html");
		std::ostream& out = resp.send();
		out << "{\"status\": " << status
			<< ",\"info\":\"Auth Fail!\"}";
		break;
	}

}


void AdminRequestHandler::DoSendRoomCard(HTTPServerRequest &req, HTTPServerResponse &resp)
{
	resp.setStatus(HTTPResponse::HTTP_OK);
	resp.setContentType("text/html");
	std::ostream& out = resp.send();

	std::map<std::string, std::string> params;
	_httpServer->getRequestArgs(req, params);

	int status = 0;

	uint64_t reqNo = 0;

	uint64_t userID = _httpServer->GetKeyValueInt64("userid", params);
	uint32_t roomcard = _httpServer->GetKeyValueInt64("roomcard", params);
	std::string orderNo = _httpServer->GetKeyValueStr("orderno", params);
	uint64_t fromUID = _httpServer->GetKeyValueInt64("from_uid", params);
	WorldUser* pUser = GameService::Instance()->GetWorldUserMgr()->getByUID(userID);
	if (pUser == NULL) //未在线
	{
		OfflineUser* offUser = GameService::Instance()->GetOfflineUserMgr()->getUserByID(userID);
		if (offUser == NULL)
		{
			status = 0;
			//未找到玩家
		}
		else
		{
			// 写入数据库
			status = 0;
			fogs::proto::msg::SendRoomCard sendPro;
			sendPro.set_user_id(userID);
			sendPro.set_cards(roomcard);
			sendPro.set_order_no(orderNo);
			sendPro.set_from_uid(fromUID);
			zSession* pDb = GameService::getMe().SessionMgr()->getDp();
			if (pDb)
			{
				pDb->sendMsgProto(::comdef::msg_ss, ::msg_maj::SendRoomCardID, sendPro);
			}
		}
	}
	else
	{
		zSession* pSs = pUser->GetSceneSession();
		if (pSs == NULL)
		{
			status = 0;

		}
		else
		{
			status = 0;
			reqNo = _httpServer->getNewNo();
			fogs::proto::msg::SendRoomCard sendPro;
			sendPro.set_user_id(userID);
			sendPro.set_cards(roomcard);
			sendPro.set_order_no(orderNo);
			sendPro.set_from_uid(fromUID);
			pSs->sendMsgProto(::comdef::msg_ss, ::msg_maj::SendRoomCardID, sendPro);
		}
	}

	out << "{\"status\":"
		<< status
		<< ",\"info\":\"Success!\",\"no\":"
		<< reqNo
		<< "}";

	out.flush();

	cout << endl
		<< "Response sent for count="
		<< " and URI=" << req.getURI() << endl;
}

void AdminRequestHandler::DoSendMail(HTTPServerRequest &req, HTTPServerResponse &resp)
{
	resp.setStatus(HTTPResponse::HTTP_OK);
	resp.setContentType("text/html");
	std::ostream& out = resp.send();

	std::map<std::string, std::string> params;
	_httpServer->getRequestArgs(req, params);

	int status = 0;
	uint64_t reqNo = 0;

	uint64_t userID = _httpServer->GetKeyValueInt64("userid", params);
	uint32_t roomcard = _httpServer->GetKeyValueInt64("roomcard", params);
	std::string title = _httpServer->GetKeyValueStr("title", params);
	std::string content = _httpServer->GetKeyValueStr("content", params);
	WorldUser* pUser = GameService::Instance()->GetWorldUserMgr()->getByUID(userID);
	if (pUser == NULL) //未在线
	{
		OfflineUser* offUser = GameService::Instance()->GetOfflineUserMgr()->getUserByID(userID);
		if (offUser == NULL)
		{
			status = 0;
			//未找到玩家
		}
		else
		{
			// 写入数据库
			status = 0;
			::fogs::proto::msg::SendMailToPlayer sendProto;
			sendProto.add_user_ids(userID);
			sendProto.set_mail_title(title);
			sendProto.set_mail_content(content);
			sendProto.set_award_type(1);
			sendProto.set_award_num(roomcard);
			zSession* pDb = GameService::getMe().SessionMgr()->getDp();
			if (pDb)
			{
				pDb->sendMsgProto(::comdef::msg_ss, ::msg_maj::SendMailID, sendProto);
			}
		}
	}
	else
	{
		zSession* pSs = pUser->GetSceneSession();
		if (pSs == NULL)
		{
			status = 0;

		}
		else
		{
			status = 0;
			::fogs::proto::msg::SendMailToPlayer sendProto;
			sendProto.add_user_ids(userID);
			sendProto.set_mail_title(title);
			sendProto.set_mail_content(content);
			sendProto.set_award_type(1);
			sendProto.set_award_num(roomcard);
			zSession* pDb = GameService::getMe().SessionMgr()->getDp();
			if (pDb)
			{
				pDb->sendMsgProto(::comdef::msg_ss, ::msg_maj::SendMailID, sendProto);
			}
		}
	}

	out << "{\"status\":"
		<< status
		<< ",\"info\":\"Success!\",\"no\":"
		<< reqNo
		<< "}";

	out.flush();

	cout << endl
		<< "Response sent for count="
		<< " and URI=" << req.getURI() << endl;
}

void AdminRequestHandler::DoSendNotice(HTTPServerRequest &req, HTTPServerResponse &resp)
{
	resp.setStatus(HTTPResponse::HTTP_OK);
	resp.setContentType("text/html");
	std::ostream& out = resp.send();

	std::map<std::string, std::string> params;
	_httpServer->getRequestArgs(req, params);

	int status = _httpServer->checkAuth(params);
	if (status != 0)
	{
		out << "{\"status\":" << status
			<< ",\"info\":\"Auth Fail!\"}";
		return;
	}

	uint32_t gameID = _httpServer->GetKeyValueInt64("gameid", params);
	uint64_t beginTime = _httpServer->GetKeyValueInt64("beginTime", params);
	uint64_t breakTime = _httpServer->GetKeyValueInt64("breakTime", params);
	uint32_t minuteTime = _httpServer->GetKeyValueInt64("minuteTime", params);
	std::string content = _httpServer->GetKeyValueStr("content", params);

	::fogs::proto::msg::SendNoticeToGame sendProto;
	zUUID zuid;
	sendProto.set_id(zuid.generate32());
	sendProto.set_begin_time(beginTime);
	sendProto.set_break_time(breakTime);
	sendProto.set_content(content);
	sendProto.set_loop_time(minuteTime);

	if (gameID)
	{
		zSession* pSs = GameService::getMe().SessionMgr()->getSs(gameID);
		if (pSs)
		{
			pSs->sendMsgProto(::comdef::msg_ss, ::msg_maj::SendNoticeID, sendProto);
			status = 0;
		}
	}
	else
	{
		struct MyStruct : public execEntry<zSession>
		{
			MyStruct(::fogs::proto::msg::SendNoticeToGame& i_sendProto) :m_sendProto(i_sendProto)
			{
			}
			virtual bool exec(zSession *entry)
			{
				if (entry->GetServerType() == config::server_t_scene)
				{
					entry->sendMsgProto(::comdef::msg_ss, ::msg_maj::SendNoticeID, m_sendProto);
				}
				return true;
			}
			::fogs::proto::msg::SendNoticeToGame& m_sendProto;
		};

		MyStruct exec(sendProto);
		GameService::getMe().SessionMgr()->execEverySession(exec);
		status = 0;
	}

	out << "{\"status\":"
		<< status
		<< ",\"info\":\"Success!\""
		<< "}";

	out.flush();

	cout << endl
		<< "Response sent for count="
		<< " and URI=" << req.getURI() << endl;
}

void AdminRequestHandler::DoIosCharge(HTTPServerRequest &req, HTTPServerResponse &resp)
{
	resp.setStatus(HTTPResponse::HTTP_OK);
	resp.setContentType("text/html");
	std::ostream& out = resp.send();

	std::map<std::string, std::string> params;
	_httpServer->getRequestArgs(req, params, true);

	struct MyStruct : public execEntry<SceneRoom>
	{
		MyStruct(std::vector<SceneRoom*>& _vecRoonList) :vecList(_vecRoonList)
		{

		}
		virtual bool exec(SceneRoom *entry)
		{
			vecList.push_back(entry);
			return true;
		}
		std::vector<SceneRoom*>& vecList;
	};


	uint64_t reqNo = 0;

	std::string order_info = _httpServer->GetKeyValueStr("order_info", params);

	JSON::Parser parser;
	parser.reset();

	Dynamic::Var result = parser.parse(order_info);
	JSON::Object::Ptr pObj = result.extract<JSON::Object::Ptr>();
	uint64_t userID = atol(pObj->get("userID").toString().c_str());

	std::string transaction_id = _httpServer->GetKeyValueStr("transaction_id", params);
	std::string purchase_date = _httpServer->GetKeyValueStr("purchase_date", params);
	std::string purchase_date_pst = _httpServer->GetKeyValueStr("purchase_date_pst", params);
	std::string product_id = _httpServer->GetKeyValueStr("product_id", params);

	int status = 0;
	int32_t roomcard = 1;
	if (product_id == "1001")
		roomcard = 3;
	else if (product_id == "1002")
		roomcard = 36;
	else if (product_id == "1003")
		roomcard = 68;
	else if (product_id == "1004")
		roomcard = 108;

	WorldUser* pUser = GameService::Instance()->GetWorldUserMgr()->getByUID(userID);
	if (pUser == NULL) //未在线
	{
		OfflineUser* offUser = GameService::Instance()->GetOfflineUserMgr()->getUserByID(userID);
		if (offUser == NULL)
		{
			status = 0;
			//未找到玩家
			H::logger->error("IOSCharge Not Found User:uid=%lld,roomcard=!", userID, roomcard);
		}
		else
		{
			// 写入数据库
			status = 0;
			fogs::proto::msg::SendRoomCard sendPro;
			sendPro.set_user_id(userID);
			sendPro.set_cards(roomcard);
			sendPro.set_order_no(transaction_id);
			sendPro.set_from_uid(0);
			zSession* pDb = GameService::getMe().SessionMgr()->getDp();
			if (pDb)
			{
				pDb->sendMsgProto(::comdef::msg_ss, ::msg_maj::SendRoomCardID, sendPro);
			}
		}
	}
	else
	{
		zSession* pSs = pUser->GetSceneSession();
		if (pSs == NULL)
		{
			status = 0;
		}
		else
		{
			status = 0;
			reqNo = _httpServer->getNewNo();
			fogs::proto::msg::SendRoomCard sendPro;
			sendPro.set_user_id(userID);
			sendPro.set_cards(roomcard);
			sendPro.set_order_no(transaction_id);
			sendPro.set_from_uid(0);
			pSs->sendMsgProto(::comdef::msg_ss, ::msg_maj::SendRoomCardID, sendPro);
		}
	}

	out << "{\"status\":"
		<< status
		<< ",\"info\":\"Success!\""
		<< "}";

	out.flush();

	cout << endl
		<< "Response sent for count="
		<< " and URI=" << req.getURI() << endl;
}

void AdminRequestHandler::DoRoomInfo(HTTPServerRequest &req, HTTPServerResponse &resp)
{
	resp.setStatus(HTTPResponse::HTTP_OK);
	resp.setContentType("text/html");
	std::ostream& out = resp.send();

	std::map<std::string, std::string> params;
	_httpServer->getRequestArgs(req, params);

	int status = 0;

	uint32_t room_id = _httpServer->GetKeyValueInt64("room_id", params);
	SceneRoom* pRoom = GameService::getMe().GetSceneRoomMgr()->getRoom(room_id);
	if (pRoom == NULL)
	{
		out << "{\"status\":"
			<< 1
			<< ",\"info\":\"Fail,Not Found!\""
			<< ",\"room_id\":"
			<< room_id
			<< "}";
	}
	else
	{
		out << "{\"status\":"
			<< 0
			<< ",\"info\":\"Success!\""
			<< ",\"room_id\":"
			<< room_id
			<< ",\"roles\":[";
		for (uint16_t i = 0; i < pRoom->GetCurPerson(); i++)
		{

		}
		out << "]"
			<< "}";
	}

	out.flush();

	cout << endl
		<< "Response sent for count="
		<< " and URI=" << req.getURI() << endl;
}

void AdminRequestHandler::DoMyRoom(HTTPServerRequest &req, HTTPServerResponse &resp)
{
	resp.setStatus(HTTPResponse::HTTP_OK);
	resp.setContentType("text/html");
	std::ostream& out = resp.send();

	std::map<std::string, std::string> params;
	_httpServer->getRequestArgs(req, params);

	int status = _httpServer->checkAuth(params);
	if (status != 0)
	{
		out << "{\"status\":" << status
			<< ",\"info\":\"Auth Fail!\"}";
		return;
	}

	uint64_t userID = _httpServer->GetKeyValueInt64("userid", params);
	uint64_t gameID = _httpServer->GetKeyValueInt64("gameid", params);

	WorldUser* wsUser = GameService::getMe().GetWorldUserMgr()->getByUID(userID);
	if (wsUser == NULL)
	{
		out << "{\"status\":"
			<< status
			<< ",\"info\":\"Not Found User!\"";
		out << ",\"room_id\":"
			<< 0
			<< "}";

		out.flush();
		return;
	}

	uint64_t usRoomID = wsUser->GetRoomID();

	out << "{\"status\":"
		<< status
		<< ",\"info\":\"Success!\"";
	out << ",\"room_id\":"
		<< usRoomID
		<< "}";

	out.flush();

	cout << endl
		<< "Response sent for count="
		<< " and URI=" << req.getURI() << endl;
}

void AdminRequestHandler::DoUserEdit(HTTPServerRequest &req, HTTPServerResponse &resp)
{
	resp.setStatus(HTTPResponse::HTTP_OK);
	resp.setContentType("text/html");
	std::ostream& out = resp.send();

	std::map<std::string, std::string> params;
	_httpServer->getRequestArgs(req, params);

	int status = 0;

	uint64_t reqNo = 0;

	uint64_t userID = _httpServer->GetKeyValueInt64("userid", params);
	uint32_t level = _httpServer->GetKeyValueInt64("level", params);
	uint32_t roomcard = _httpServer->GetKeyValueInt64("roomcard", params);
	uint64_t fromUID = _httpServer->GetKeyValueInt64("from_uid", params);
	WorldUser* pUser = GameService::Instance()->GetWorldUserMgr()->getByUID(userID);
	if (pUser == NULL) //未在线
	{
		OfflineUser* offUser = GameService::Instance()->GetOfflineUserMgr()->getUserByID(userID);
		if (offUser == NULL)
		{
			status = 0;
			//未找到玩家
		}
		else
		{
			// 写入数据库
			status = 0;
			fogs::proto::msg::UserEditCard sendPro;
			sendPro.set_user_id(userID);
			sendPro.set_cards(roomcard);
			sendPro.set_level(level);
			sendPro.set_from_uid(fromUID);
			zSession* pDb = GameService::getMe().SessionMgr()->getDp();
			if (pDb)
			{
				pDb->sendMsgProto(::comdef::msg_ss, ::msg_maj::UserEditCardID, sendPro);
			}
		}
	}
	else
	{
		zSession* pSs = pUser->GetSceneSession();
		if (pSs == NULL)
		{
			status = 0;

		}
		else
		{
			status = 0;
			fogs::proto::msg::UserEditCard sendPro;
			sendPro.set_user_id(userID);
			sendPro.set_cards(roomcard);
			sendPro.set_level(level);
			sendPro.set_from_uid(fromUID);
			pSs->sendMsgProto(::comdef::msg_ss, ::msg_maj::UserEditCardID, sendPro);
		}
	}

	out << "{\"status\":"
		<< status
		<< ",\"info\":\"Success!\""
		<< "}";

	out.flush();

	cout << endl
		<< "Response sent for count="
		<< " and URI=" << req.getURI() << endl;
}

void AdminRequestHandler::DoExitGame(HTTPServerRequest &req, HTTPServerResponse &resp)
{

	// 通知login,不再让新登录
	zSession* ls = GameService::getMe().SessionMgr()->getLs();
	if (ls)
	{
		::msg_maj::ServerBrepairingNotify sendLogin;
		sendLogin.set_status(1);
		ls->sendMsgProto(::comdef::msg_ss, ::msg_maj::ServerIsBrepairingNotifyID, sendLogin);
	}

	resp.setStatus(HTTPResponse::HTTP_OK);
	resp.setContentType("text/html");
	std::ostream& out = resp.send();

	std::map<std::string, std::string> params;
	_httpServer->getRequestArgs(req, params);

	int status = 0;

	WorldUserMgr* userMgr =	GameService::Instance()->GetWorldUserMgr();

	struct MyStruct : execEntry<WorldUser>
	{
		virtual bool exec(WorldUser *entry)
		{
			::msg_maj::KictoutResp kictResp;
			kictResp.set_code(::msg_maj::SERVER_IS_BREPAIRING);
			entry->sendMsgToFep(::comdef::msg_login, ::msg_maj::kictout_resp, kictResp);
			return true;
		}
	};

	MyStruct exec;
	userMgr->execEveryUser(exec);

	out << "{\"status\":"
		<< status
		<< ",\"info\":\"Success!\""
		<< "}";

	out.flush();

	cout << endl
		<< "Response sent for count="
		<< " and URI=" << req.getURI() << endl;
}

void AdminRequestHandler::DoReloadConfig(HTTPServerRequest &req, HTTPServerResponse &resp)
{
	// 通知login,不再让新登录
	zSession* ss = GameService::getMe().SessionMgr()->getSs(reload_config_product_id);
	if (ss == NULL)
	{
		return;
	}

	resp.setStatus(HTTPResponse::HTTP_OK);
	resp.setContentType("text/html");
	std::ostream& out = resp.send();

	// 
	fogs::proto::msg::RefreshConfig proto;
	ss->sendMsgProto(::comdef::msg_ss, ::msg_maj::RefreshConfigID, proto);

	//std::map<std::string, std::string> params;
	//_httpServer->getRequestArgs(req, params);

	int status = 0;

	out << "{\"status\":"
		<< status
		<< ",\"info\":\"Success!\""
		<< "}";

	out.flush();

	cout << endl
		<< "Response sent for count="
		<< " and URI=" << req.getURI() << endl;
}

void AdminRequestHandler::DoSceneList(HTTPServerRequest &req, HTTPServerResponse &resp)
{
	resp.setStatus(HTTPResponse::HTTP_OK);
	resp.setContentType("text/html");
	std::ostream& out = resp.send();

	std::vector<zSession*> vecSession;
	GameService::Instance()->SessionMgr()->getSsList(vecSession);

	int status = 0;

	out << "{\"status\":"
		<< status
		<< ",\"info\":\"Success!\"";
	out << ",\"srv_list\":[";
	int count = 0;
	for (std::vector<zSession*>::iterator it = vecSession.begin(); it != vecSession.end(); ++it)
	{
		zSession* s = *it;

		if (count > 0) out << ",";

		out << "{\"id\":";
		out << s->GetRemoteServerID();
		out << ",";
		out << "\"name\":";
		out << "\"" << s->GetRemoteServerName() << "\"";
		out << "}";
		count++;
	}
	out << "]";
	out << "}";

	out.flush();

	cout << endl
		<< "Response sent for count="
		<< " and URI=" << req.getURI() << endl;
}

