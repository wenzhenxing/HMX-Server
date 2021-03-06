#include "NetMsgHandler.h"
#include "GameService.h"
#include "ProcDpHandler.h"
#include "ProcFepHandler.h"
#include "ProcWorldHandler.h"
#include "CWorld.hpp"

NetMsgHandler::NetMsgHandler() :zMsgHandler(GameService::Instance())
{
	/*  从WS过来的业务协议 */
#define REGISTER_W2S_MESSAGE(cmd,cmdType,cls,handler) \
	{\
		RegisterMessage(cmd,cmdType, sizeof(cls), boost::bind(&ProcWorldHandler::handler, ProcWorldHandler::Instance(), _1, _2, _3)); \
	}
	
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::TransterToSceneReqID, PbMsgWebSS, TransTerToSceneReq);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::TransterToSceneRespID, PbMsgWebSS, TransTerToSceneResp);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::login_to_scene_req, PbMsgWebSS, HandlerLoginScene);	/* 请求进入场景 */
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::QueryRankResponseID, PbMsgWebSS, HandleQueryRankResponse);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::UpdateRankSortResponseID, PbMsgWebSS, HandleUpdateRankResponse);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::SendRoomCardID, PbMsgWebSS, HandleSendRoomCard);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::UserEditCardID, PbMsgWebSS, HandleUserEditCard);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::SendEditRoomCardID, PbMsgWebSS, HandleEditRoomCard);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::SendMailID, PbMsgWebSS, HandleSendMail);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::SendNoticeID, PbMsgWebSS, HandleSendNotice);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::DeleteNoticeID, PbMsgWebSS, HandleDeleteNotice);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::SendBlockUserID, PbMsgWebSS, HandleSendBlock);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::GetBindingAgentInfoRespID, PbMsgWebSS, HandleBindingAgentInfoResp);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::BindingAgentRespID, PbMsgWebSS, HandleBindingAgentResp);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::InputInviteCodeRespID,PbMsgWebSS, HandleInputInviteCodeResp);
	REGISTER_W2S_MESSAGE(::comdef::msg_ss, ::msg_maj::RefreshConfigID, PbMsgWebSS, HandleRefreshConfigResp);

#undef REGISTER_W2S_MESSAGE

	// dp 
#define REGISTER_DP_MESSAGE(cmd,cmdType,cls,handler)\
	{\
	RegisterMessage(cmd,cmdType, sizeof(cls), boost::bind(&ProcDpHandler::handler, ProcDpHandler::Instance(), _1, _2, _3)); \
	}

	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::QueryPlayerResponseID, PbMsgWebSS, HandleQueryPlayerResponse);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::QueryMailResponseID, PbMsgWebSS, HandleQueryMailResponse);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::QueryMailSysLogResponseID, PbMsgWebSS, HandleQueryMailSysLogResponse);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::GetMaxUUIDResponseID, PbMsgWebSS, HandleGetMaxUUIDResponse);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::SessionAccountNameID, PbMsgWebSS, HandleSessionAccountName);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::UpdateRoomInfoID, PbMsgWebSS, HandleUpdateRoomInfo);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::ZhanjiQueryListResponseID, PbMsgWebSS, HandlerZhanjiQueryResp);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::ZhanjiQueryRoomResponseID, PbMsgWebSS, HandlerZhanjiRoomResp);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::ZhanjiRespReplyResponseID, PbMsgWebSS, HandlerZhanjiReplyResp);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::GetFreeConfigRespID, PbMsgWebSS, HandleFreeConfigResp);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::GetRewardConfigRespID, PbMsgWebSS, HandleRewardConfigResp);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::GetNotifyConfigRespID, PbMsgWebSS, HandleNotifyConfigResp);
	REGISTER_DP_MESSAGE(::comdef::msg_ss, ::msg_maj::GetRoomSetConfigRespID, PbMsgWebSS, HandleRoomSetConfigResp);

#undef REGISTER_DP_MESSAGE

		// ls
#define REGISTER_LS_MESSAGE(msg_idx,cls,handler)\
	{\
	RegisterMessage(msg_idx, sizeof(cls), boost::bind(&ProcLsHandler::handler, ProcLsHandler::Instance(), _1, _2, _3)); \
	}

//	REGISTER_LS_MESSAGE(PRO_L2W_LOADLIST, L2WLoadList, RqLoadList);

#undef REGISTER_LS_MESSAGE

	/* 从FEP的协议 */
#define REGISTER_FEP_MESSAGE(cmd,cmdType,cls,handler)\
	{\
	RegisterMessage(cmd,cmdType, sizeof(cls), boost::bind(&ProcFepHandler::handler, ProcFepHandler::Instance(), _1, _2, _3)); \
	}\

	//-----login-------
	REGISTER_FEP_MESSAGE(::comdef::msg_login, ::msg_maj::notify_dis_conntion, PbMsgWebSS, HandlePlayerExit);

	//-----role--------
	REGISTER_FEP_MESSAGE(::comdef::msg_role, ::msg_maj::player_info_req, PbMsgWebSS, HandleRoleReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_role, ::msg_maj::gps_upload_req, PbMsgWebSS, HandleGpsUploadReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_role, ::msg_maj::update_role_req, PbMsgWebSS, HandleUpdateReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_role, ::msg_maj::cardinfo_req, PbMsgWebSS, HandleCardInfoReq);

	//-----room--------
	REGISTER_FEP_MESSAGE(::comdef::msg_room, ::msg_maj::room_list_req, PbMsgWebSS, HandleRoomListReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_room, ::msg_maj::open_room_req, PbMsgWebSS, HandleOpenRoomReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_room, ::msg_maj::enter_room_req, PbMsgWebSS, HandleEnterRoomReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_room, ::msg_maj::leave_room_req, PbMsgWebSS, HandleLeaveRoomReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_room, ::msg_maj::dismiss_room_req, PbMsgWebSS, HandleDismissRoomReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_room, ::msg_maj::room_reconnect_req, PbMsgWebSS, HandleReconnectReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_room, ::msg_maj::dismiss_room_vote_accept, PbMsgWebSS, HandleDismissAcceptReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_room, ::msg_maj::kick_role_req, PbMsgWebSS, HandleKickRoleReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_room, ::msg_maj::enter_room_ready, PbMsgWebSS, HandleEnterRoomReady);
	REGISTER_FEP_MESSAGE(::comdef::msg_room, ::msg_maj::roomcard_price_req, PbMsgWebSS, HandleRoomCardPriceReq);

	//-----maj---------
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::start_game_req, PbMsgWebSS, HandleStartGameReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::accept_start_req, PbMsgWebSS, HandleAcceptStartReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::disover_card_req, PbMsgWebSS, HandleDisoverCardReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::discard_tile_req, PbMsgWebSS, HandleDiscardTileReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::prepare_round_req, PbMsgWebSS, HandlePrepareRoundReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::response_req, PbMsgWebSS, HandleResponseReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::reconnect_req, PbMsgWebSS,HandleReconnectReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::reconnect_ready_req, PbMsgWebSS, HandleReconnectReadyReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::reconnect_other_ready_req, PbMsgWebSS, HandleReconnectOtherReadyReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::quickmessage_req, PbMsgWebSS, HandlerQuickMessageReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::huan_sanzhang_req, PbMsgWebSS, HandlerQuickSanZhangReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::dingque_req, PbMsgWebSS, HandlerQuickDingQueReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::my_scorelist_req, PbMsgWebSS, HandlerQuickMyScoreListReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_maj, ::msg_maj::my_ting_pai_req, PbMsgWebSS, HandlerQuickMyTingListReq);

	//-----rank-------
	REGISTER_FEP_MESSAGE(::comdef::msg_rank, ::msg_maj::rank_req, PbMsgWebSS, HandleRankTopList);


	//-------gm--------
	REGISTER_FEP_MESSAGE(::comdef::msg_gm, ::msg_maj::remaining_card_req, PbMsgWebSS, HandleRemainingCardReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_gm, ::msg_maj::assign_all_cards_req, PbMsgWebSS, HandleAssignAllCardsReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_gm, ::msg_maj::assign_next_card_req, PbMsgWebSS, HandleAssignNextCardReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_gm, ::msg_maj::gm_common_oper_req, PbMsgWebSS, HandleCommonReq);

	//-------event--------
	REGISTER_FEP_MESSAGE(::comdef::msg_event, ::msg_event::mail_list_req, PbMsgWebSS, HandleListReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_event, ::msg_event::mail_opt_req, PbMsgWebSS, HandleOptReq);

	//-------hist--------
	REGISTER_FEP_MESSAGE(::comdef::msg_hist, ::msg_maj::history_list_req, PbMsgWebSS, HandleHistoryReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_hist, ::msg_maj::replay_req, PbMsgWebSS, HandleReplayReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_hist, ::msg_maj::history_room_req, PbMsgWebSS, HandleHistRoomReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_hist, ::msg_maj::history_inn_req, PbMsgWebSS, HandleHistInnReq);

	//-------activity----
	REGISTER_FEP_MESSAGE(::comdef::msg_activity, ::msg_maj::get_share_info_req, PbMsgWebSS, HandleGetShareInfoReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_activity, ::msg_maj::share_Report_req, PbMsgWebSS, HandleShareReportReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_activity, ::msg_maj::Recv_share_reward_req, PbMsgWebSS, HandleRecvShareRewardReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_activity, ::msg_maj::get_invitation_info_req, PbMsgWebSS, HandleGetInvitationInfoReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_activity, ::msg_maj::Recv_invitation_reward_req, PbMsgWebSS, HandleRecvInvitationRewardReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_activity, ::msg_maj::get_binding_agent_info_req, PbMsgWebSS, HandleGetBindingAgentInfoReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_activity, ::msg_maj::binding_agent_req, PbMsgWebSS, HandleBindingAgentReq);
	REGISTER_FEP_MESSAGE(::comdef::msg_activity, ::msg_maj::input_invitation_code_req, PbMsgWebSS, HandleInputInvitationCodeReq);


#undef REGISTER_FEP_MESSAGE

}


NetMsgHandler::~NetMsgHandler()
{ 
}

void NetMsgHandler::OnNetMsgEnter(NetSocket& rSocket)
{
	CommonOnNetMsgEnter(rSocket);
}

void NetMsgHandler::OnNetMsg(NetSocket& rSocket, NetMsgSS* pMsg, int32_t nSize)
{
	CommonOnNetMsg(rSocket, pMsg, nSize);
}

void NetMsgHandler::OnNetMsgExit(NetSocket& rSocket)
{
	CommonOnNetMsgExit(rSocket);
}

void NetMsgHandler::NewLoginSuccessed(zSession* pSession, int32_t server_id, int32_t server_type)
{
	if (server_type == 5)
	{
		CWorld::Instance()->LoadConfigFromDB();
	}
}


