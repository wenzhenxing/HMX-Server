#include "ProcWorldHandler.h"

#include "SceneUserMgr.h"
#include "GameService.h"
#include "CWorld.hpp"
#include "CPlayer.hpp"
#include "CMailMgr.hpp"
#include "CNotice.hpp"
#include "CNoticeMgr.hpp"

ProcWorldHandler::ProcWorldHandler()
{
}


ProcWorldHandler::~ProcWorldHandler()
{

}

void ProcWorldHandler::TransTerToSceneReq(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}

void ProcWorldHandler::TransTerToSceneResp(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{

}

void ProcWorldHandler::HandlerLoginScene(zSession* pSession, const PbMsgWebSS* pMsg,int32_t nSize)
{
	::msg_maj::LoginToScene proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	zSession* dp = GameService::getMe().SessionMgr()->getDp();
	if (dp == NULL)
	{
		ASSERT(dp);
		return;
	}

	uint64_t newClientSessID = proto.new_session_id() > 0 ? proto.new_session_id() : pMsg->clientSessID;

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerByUUID(proto.uid());
	if (pPlayer == NULL)
	{
		fogs::proto::msg::QueryPlayerRequest proReq;
		proReq.set_uid(proto.uid());
		proReq.set_room_id(proto.join_room_id());
		dp->sendMsgProto(::comdef::msg_ss, ::msg_maj::QueryPlayerRequestID, newClientSessID ,pMsg->fepServerID, proReq);
	}
	else
	{
		pPlayer->m_usLoginTime = time(NULL);
		pPlayer->SendNotifyAnotherLogin();
		pPlayer->SetDisconnect(true);
		CWorld::Instance()->ResetSessionID(pPlayer, newClientSessID);

		pPlayer->SetSessionID(newClientSessID);
		pPlayer->UpdateToGate();
		pPlayer->EnterGame();
		pPlayer->SaveDataToDB(0);

		if (proto.join_room_id() > 0)
		{
			pPlayer->JoinRoom(proto.join_room_id());
		}
	}
}

void ProcWorldHandler::HandleQueryRankResponse(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::QueryRankReqResp proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerByUUID(proto.char_id());
	if (NULL == pPlayer)
	{
		return;
	}
	pPlayer->SendMsgToClient(::comdef::msg_rank, ::msg_maj::rank_resp, proto.ranklist());
}

void ProcWorldHandler::HandleUpdateRankResponse(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::UpdatePlayerSort proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerByUUID(proto.char_id());
	if (NULL == pPlayer)
	{
		return;
	}

	if (proto.ranktype() == ::msg_maj::rank_t_wins)
	{
		pPlayer->SetTopWinSort(proto.sort());
	}
	else if (proto.ranktype() == ::msg_maj::rank_t_score)
	{
		pPlayer->SetTopScoreSort(proto.sort());
	}
}

void ProcWorldHandler::HandleSendRoomCard(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::SendRoomCard proReq;
	proReq.ParseFromArray(pMsg->data, pMsg->size);
	DUMP_PROTO_MSG(proReq);

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerByUUID(proReq.user_id());
	if (NULL == pPlayer)
	{
		CMailMgr::Instance()->SendToPlayer(0, "system", proReq.user_id(), "", "title", "roomcards", proReq.cards());
		GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::SendRoomCardID, proReq);
	}
	else
	{
		if (proReq.cards() > 0)
		{
			pPlayer->AddRoomCards(proReq.cards(), ::fogs::proto::msg::log_t_roomcard_add_charge);
			pPlayer->SendRoomCards();
			fogs::proto::msg::RechargeRoomCard proData;
			proData.set_order_no(proReq.order_no());
			proData.set_user_id(proReq.user_id());
			proData.set_room_card(proReq.cards());
			proData.set_is_add(1);
			proData.set_time(time(NULL));
			proData.set_from_uid(proReq.from_uid());
			GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::RechargeRoomCardID, proData);
		}
	}
}

void ProcWorldHandler::HandleUserEditCard(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::UserEditCard proReq;
	proReq.ParseFromArray(pMsg->data, pMsg->size);
	DUMP_PROTO_MSG(proReq);

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerByUUID(proReq.user_id());
	if (NULL == pPlayer)
	{
		GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::UserEditCardID, proReq);
	}
	else
	{
		if (proReq.cards() > 0)
		{
			pPlayer->SetRoomCard(proReq.cards());
			pPlayer->SetLevel(proReq.level());
			pPlayer->SendRoomCards();
		}
	}
}

void ProcWorldHandler::HandleEditRoomCard(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::EditRoomCard proReq;
	proReq.ParseFromArray(pMsg->data, pMsg->size);
	DUMP_PROTO_MSG(proReq);

	if (proReq.roomcard_num() == 0)
	{
		return;
	}

	CPlayer* pPlayer = CWorld::Instance()->GetPlayerByUUID(proReq.user_id());
	if (NULL == pPlayer)
	{
		//GameService::Instance()->SendToDp(::comdef::msg_ss,::msg_maj:: SendEditRoomCardID, 0, proReq);
	}
	else
	{
		if (proReq.roomcard_num() > 0)
		{
			pPlayer->AddRoomCards(proReq.roomcard_num(), ::fogs::proto::msg::log_t_roomcard_add_admin);
			pPlayer->SendRoomCards();
		}
		else if (proReq.roomcard_num() < 0)
		{
			pPlayer->SubRoomCards(proReq.roomcard_num(), ::fogs::proto::msg::log_t_roomcard_sub_admin);
			pPlayer->SendRoomCards();
		}
	}
}

void ProcWorldHandler::HandleSendMail(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::SendMailToPlayer proReq;
	proReq.ParseFromArray(pMsg->data, pMsg->size);
	DUMP_PROTO_MSG(proReq);
	for (int i = 0; i < proReq.user_ids_size(); ++i)
	{
		int64_t uid = proReq.user_ids(i);
		if (uid > 0)
		{
			CMailMgr::Instance()->SendToPlayer(0, "system", (uint64_t)uid, "", proReq.mail_title(), proReq.mail_content(), proReq.award_num());
			CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
			if (pPlayer) CMailMgr::Instance()->SendMailList(pPlayer, 0);
		}
		else if (uid == -1)
		{
			CMailMgr::Instance()->SendToPlayer(0, "common", (uint64_t)0, "", proReq.mail_title(), proReq.mail_content(), proReq.award_num());
		}
	}
}

void ProcWorldHandler::HandleDeleteNotice(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	::fogs::proto::msg::DeleteNoticeToGame proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	CNotice* pNotice = CNoticeMgr::Instance()->GetNotice(proto.id());
	if (pNotice)
	{
		CNoticeMgr::Instance()->Remove(pNotice);
		CNoticeMgr::Instance()->SendNoticeList(NULL);
	}
}

void ProcWorldHandler::HandleSendNotice(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::SendNoticeToGame proReq;
	proReq.ParseFromArray(pMsg->data, pMsg->size);

	::msg_event::NoticeS notice;
	notice.set_id(proReq.id());
	notice.set_begin_time(proReq.begin_time());
	notice.set_break_time(proReq.break_time());
	notice.set_minute_time(proReq.loop_time());
	notice.set_content(proReq.content());
	notice.set_sort_level(99);
	CNoticeMgr::Instance()->AddRecord(notice, true);
}

void ProcWorldHandler::HandleSendBlock(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::SendBlockUser proReq;
	proReq.ParseFromArray(pMsg->data, pMsg->size);
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerByUUID(proReq.user_id());
	if (pPlayer)
	{
		pPlayer->SetBlockTime(proReq.closuretime());
	}
	else // 上传到db中操作数据库
	{
		//GameService::Instance()->SendToDp(::comdef::msg_ss,::msg_maj:: SendBlockUserID, 0, proReq);
	}
}

void ProcWorldHandler::HandleBindingAgentInfoResp(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	fogs::proto::msg::GetBindingAgentInfoResp proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	::msg_maj::GetBindingAgentInfoResp sendData;

	sendData.set_user_id(proto.agent_id());
	sendData.set_name(proto.agent_name());
	sendData.set_icon(proto.agent_icon());

	pPlayer->SendMsgToClient(::comdef::msg_activity, ::msg_maj::get_binding_agent_info_resp, sendData);
}

void ProcWorldHandler::HandleBindingAgentResp(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	fogs::proto::msg::BindingAgentResp proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	::msg_maj::BindingAgentResp sendProto;
	sendProto.set_code(proto.result());
	sendProto.set_user_id(proto.agent_id());
	sendProto.set_name(proto.agent_name());
	sendProto.set_icon(proto.agent_icon());
	sendProto.set_code_info(proto.code_info());
	
	pPlayer->SendMsgToClient(::comdef::msg_activity, ::msg_maj::binding_agent_resp, sendProto);
}

void ProcWorldHandler::HandleInputInviteCodeResp(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	CPlayer* pPlayer = CWorld::Instance()->GetPlayerBySessionID(pMsg->clientSessID);
	if (NULL == pPlayer)
	{
		return;
	}

	fogs::proto::msg::InputInviteCodeResp proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);

	::msg_maj::InputInvitationCodeResp sendProto;
	sendProto.set_code(proto.result());
	sendProto.set_reward_room_card(proto.room_card());
	sendProto.set_code_info(proto.code_info());
	pPlayer->SendMsgToClient(::comdef::msg_activity, ::msg_maj::input_invitation_code_resp, sendProto);
}

void ProcWorldHandler::HandleRefreshConfigResp(zSession* pSession, const PbMsgWebSS* pMsg, int32_t nSize)
{
	fogs::proto::msg::RefreshConfig proto;
	proto.ParseFromArray(pMsg->data, pMsg->size);
	
	CWorld::Instance()->LoadConfigFromDB();
}


