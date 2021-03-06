#include "CRoom.hpp"
#include "CPlayer.hpp"
#include "CWorld.hpp"
#include "CMaJiang.hpp"
#include "CRoomMgr.hpp"

#include "CMajingRuleYL.hpp"
#include "CMajingRuleBB.hpp"
#include "CMajingRuleLC.hpp"
#include "CMajingRuleXZ.hpp"
#include "CMajingRuleXL.hpp"

#include "COnlineRecord.h"
#include "CHuScore.h"

#include "GameService.h"

::boost::object_pool<CRoom>	g_objpoolRoom;

CRoom::CRoom(::msg_maj::RoomType roomType, ::msg_maj::maj_type majType, const ::msg_maj::RoomOption& option) :
	m_bTest(false),
	m_usGameType(majType),
	m_eRoomType(roomType),
	m_unRoomID(0),
	m_ulRoomer(0),
	m_roomOption(option),
	m_eRoomStatus(eRoomStatus_Ready),
	m_usMaxMultiType(0),
	m_nMaxMultiNum(9999),
	m_bStart(false),
	m_bClickDissmiss(false),
	m_bClose(false),
	m_usBankerSeat(0),
	m_bRobot(false),
	m_usLimitTime(0),
	m_unRecordID(0),
	m_unZjRoomID(0),
	m_reconrdTime(0),
	m_nRoomType(0)
{
	zUUID zuuid;
	m_ulRoomUUID = zuuid.generate32();
	m_unRoomID = CRoomMgr::Instance()->GenerateRoomID();
	m_pMaj = g_objpoolMaJiang.construct(this);

	m_pTimerSendCard = new fogs::FogsTimer(*GameService::Instance()->GetIoService());
	m_pTimerClose = new fogs::FogsTimer(*GameService::Instance()->GetIoService());

	switch (m_usGameType)
	{
	case msg_maj::maj_t_yulin:m_pRule = new CMajingRuleYL(this, option);break;
	case msg_maj::maj_t_bobai:m_pRule = new CMajingRuleBB(this, option);break;
	case msg_maj::maj_t_luchuan:m_pRule = new CMajingRuleLC(this, option);break;
	case msg_maj::maj_t_xiezhan:m_pRule = new CMajingRuleXZ(this, option); break;
	case msg_maj::maj_t_xieliu:m_pRule = new CMajingRuleXL(this, option); break;
	default: 
		ASSERT(0);
		break;
	}

	m_pMaj->SetRule(m_pRule);
	m_pRule->SetMajiang(m_pMaj);

	m_usRoomPersons = option.renshutype() == 1 ? 4 : option.renshutype() == 2 ? 3 : 2;
	
	m_vecPlayer.resize(m_usRoomPersons);

	switch (m_roomOption.jushutype())
	{
	case 1: m_nGames = 8; break;
	case 2: m_nGames = 16; break;
	case 3: m_nGames = 24; break;
	default: m_nGames = 8; break;
	}
	m_nAllGames = m_nGames;
	m_roomOption.set_total_pai_num(m_pRule->GetTotalPaiNum());

	m_bRobot = CWorld::Instance()->GetRoomSetConfig().start_robot() == 1;

	int32_t limit_time = CWorld::Instance()->GetRoomSetConfig().limit_time();
	m_usLimitTime = (limit_time > 0) ? (time(NULL) + limit_time): 0;
	
	//记录游戏信息
	AddRoomInfo(m_ulRoomUUID, m_unRoomID, m_eRoomType, m_roomOption, time(NULL));
}

CRoom::~CRoom()
{
	Release();
}

void CRoom::Release()
{
	g_objpoolMaJiang.destroy(m_pMaj);

	if (m_pTimerSendCard)
	{
		m_pTimerSendCard->cancel();
		SAFE_DELETE(m_pTimerSendCard);
	}
	if (m_pTimerClose)
	{
		m_pTimerClose->cancel();
		SAFE_DELETE(m_pTimerClose);
	}
}

void CRoom::StartGameReq(CPlayer* pPlayer)
{
	if (m_bStart || pPlayer->GetCharID() != m_ulRoomer)
	{
		return;
	}

	if (!CheckStart())
	{
		return;
	}

	m_eRoomStatus = eRoomStatus_ClickStart;

	::msg_maj::AskStartGame proData;
	std::map<std::string, std::vector<uint16_t> > samelist;
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i])
		{
			continue;
		}

		::msg_maj::GpsSeatInfo* seatInfo = proData.add_seat_gps_list();
		if (seatInfo)
		{
			seatInfo->set_seat_id(m_vecPlayer[i]->GetSeat());
			seatInfo->set_longitude(m_vecPlayer[i]->GetGpsInfo().longitude);
			seatInfo->set_latitude(m_vecPlayer[i]->GetGpsInfo().latitude);
		}

		std::string strIP = CWorld::Instance()->GetSessionIP(m_vecPlayer[i]->GetSessionID());
		if (strIP.length() == 0)
		{
			continue;
		}
		std::map<std::string, std::vector<uint16_t> >::iterator iter = samelist.find(strIP);
		if (iter == samelist.end())
		{
			std::vector<uint16_t> seatlist;
			seatlist.push_back(i);
			samelist.insert(std::make_pair(strIP, seatlist));
		}
		else
		{
			(iter->second).push_back(i);
		}
	}
	for (std::map<std::string, std::vector<uint16_t> >::iterator iter = samelist.begin(); iter != samelist.end(); ++iter)
	{
		std::vector<uint16_t>& seatlist = iter->second;
		if (seatlist.size() >= 2)
		{
			::msg_maj::SameIpSeats* pPro = proData.add_same_ips();
			for (std::vector<uint16_t>::iterator it = seatlist.begin(); it != seatlist.end(); ++it)
			{
				pPro->add_seat(*it);
			}
		}
	}
	proData.set_timer_sec(15);
	BrocastMsg(::comdef::msg_maj, ::msg_maj::ask_start_game, proData);
}

bool CRoom::CheckStart()
{
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		CPlayer* cpy = m_vecPlayer[i];
		if (NULL == cpy )
			return false;
		if (cpy->IsDisconnect())
			return false;
		if (!cpy->IsEnterRoomReady())
			return false;
	}
	return true;
}

void CRoom::SendStartButton()
{
	msg_maj::RoomReadyNotify proStart;
	proStart.set_roomer_aciton(msg_maj::can_start);
	if (!m_vecPlayer[0]->IsDisconnect())
	{
		m_vecPlayer[0]->SendMsgToClient(::comdef::msg_room, ::msg_maj::room_ready_notify, proStart);
	}
}

void CRoom::SendCancelStartButton()
{
	msg_maj::RoomReadyNotify proStart;
	proStart.set_roomer_aciton(msg_maj::cancel_start);
	if (!m_vecPlayer[0]->IsDisconnect())
	{
		m_vecPlayer[0]->SendMsgToClient(::comdef::msg_room, ::msg_maj::room_ready_notify, proStart);
	}
}

void CRoom::SendHandCards()
{
	RecordPlayerInfo();

	m_pMaj->InitMajing();

	// 检查是否有人买马
	if (m_pRule->HasMaiMa()) m_pMaj->InitMaPai();

	SendRoomInnInfo();

	std::vector<std::vector<uint16_t> > arrPaiList;
	arrPaiList.resize(m_usRoomPersons);
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i])
		{
			//LOG(ERROR) << "CRoom::SendCard()";
			return;
		}

		for (uint16_t j = 0; j < 13; ++j)
		{
			uint16_t usPai = m_pMaj->GetPai();
			m_vecPlayer[i]->AddPai(usPai);
			arrPaiList[i].push_back(usPai);
		}
	}

	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i])
		{
			continue;
		}
		::msg_maj::StartRoundResp proRep;
		proRep.set_dice(m_pMaj->m_unRoll);
		const std::vector<uint16_t>& pailist = arrPaiList[i];
		for (std::vector<uint16_t>::const_iterator iter = pailist.begin(); iter != pailist.end(); ++iter)
		{
			proRep.add_tile_list(*iter);
		}
		proRep.set_banker_seat_id(m_usBankerSeat);
		proRep.add_events(::msg_maj::deal);
		m_vecPlayer[i]->SendMsgToClient(::comdef::msg_maj, ::msg_maj::start_round_resp, proRep);
	}

	RecordHandTiles();

	m_eRoomStatus = m_pRule->SendHandCardsAllNextState();

}

void CRoom::RecordHandTiles()
{
	//开局记录
	AddInnRecord(m_nAllGames - m_nGames + 1, m_usBankerSeat, m_pMaj->m_unRoll);

	// 获得上一局的信息
	bool lastRecord = false;
	std::map<uint16_t, int16_t> mapLastSeatScore;
	GetAllInnRecordSeatScore(mapLastSeatScore);

	//发完牌记录座位数据
	std::vector<stSeatInfo> seatinfolist;
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i])
		{
			continue;
		}

		stSeatInfo st;
		st.seat = i;
		const std::vector<uint16_t> hand_tiles = m_vecPlayer[i]->GetPaiList();
		for (std::vector<uint16_t>::const_iterator iter = hand_tiles.begin(); iter != hand_tiles.end(); ++iter)
		{
			st.hand_tiles.push_back(*iter);
		}
		st.pstatus = m_vecPlayer[i]->IsDisconnect() ? ::msg_maj::disconnect : ::msg_maj::normal;
		st.score = mapLastSeatScore.empty() ? 0 : mapLastSeatScore[i]; // 取上所有局的分	
		st.dingque = m_vecPlayer[i]->m_usDingQueType;
		seatinfolist.push_back(st);
	}
	AddSeatInfo(m_nAllGames - m_nGames + 1, seatinfolist);
}

void CRoom::SendAskSanZhang(CPlayer* pPlayer)
{
	::msg_maj::AskSanZhang proRep;
	if (pPlayer)
	{
		proRep.set_lesstype(pPlayer->GetSanZhangLessType());
		pPlayer->SendMsgToClient(::comdef::msg_maj, ::msg_maj::ask_start_sanzhang, proRep);
	}
	else
	{
		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (NULL == m_vecPlayer[i] || m_vecPlayer[i]->IsDisconnect()) continue;

			if (!m_vecPlayer[i]->IsEnterRoomReady()) continue;

			if (m_vecPlayer[i]->IsSanZhanged()) continue;

			proRep.set_lesstype(m_vecPlayer[i]->GetSanZhangLessType());
			m_vecPlayer[i]->SendMsgToClient(::comdef::msg_maj, ::msg_maj::ask_start_sanzhang, proRep);
		}
	}
}

void CRoom::SendAskDingQue(CPlayer* pPlayer)
{
	
	if (pPlayer)
	{
		::msg_maj::AskDingQue proRep;
		proRep.set_lesstype(pPlayer->GetDingQueLessType());
		pPlayer->SendMsgToClient(::comdef::msg_maj, ::msg_maj::ask_start_dingque, proRep);
	}
	else
	{
		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (NULL == m_vecPlayer[i] || m_vecPlayer[i]->IsDisconnect()) continue;

			if (!m_vecPlayer[i]->IsEnterRoomReady()) continue;

			if (m_vecPlayer[i]->IsDingQueed()) continue;

			::msg_maj::AskDingQue proRep;
			proRep.set_lesstype(m_vecPlayer[i]->GetDingQueLessType());
			m_vecPlayer[i]->SendMsgToClient(::comdef::msg_maj, ::msg_maj::ask_start_dingque, proRep);
		}
	}
}

void CRoom::ExchangeSanZhangPai()
{
	uint32_t huanType = 0;
	uint32_t usRandRoll = m_pMaj->GetRandRoll();
	switch (usRandRoll)
	{
	case 11:
	case 12:
	case 21:
	case 26:
	case 62:
	case 35:
	case 53:
	case 36:
	case 63:
	case 44:
	case 56:
	case 65:
	case 66:
	{
		huanType = 0;
		break;
	}
	case 13:
	case 31:
	case 14:
	case 41:
	case 22:
	case 23:
	case 32:
	case 45:
	case 54:
	case 46:
	case 64:
	case 55:
	{
		huanType = 1;
		break;
	}
	default:
		huanType = 2;
		break;
	}

	// 逆时针
	if (huanType == 0)
	{
		for (int i = 0; i < 4; ++i)
		{
			::msg_maj::HuanSanZhangNotify sendPro;
			uint16_t getType = 0;
			CPlayer* ply = m_vecPlayer[i];
			CPlayer* plyNext = m_vecPlayer[(i + 1) % 4];
			for (int j = 0; j < ply->m_vecSanZhang.size(); ++j)
			{
				getType = ply->m_vecSanZhang[j] / 10;
				plyNext->AddPai(ply->m_vecSanZhang[j]);
				sendPro.add_pais(ply->m_vecSanZhang[j]);
			}
			for (int j = 0; j < ply->m_vecSanZhang.size(); ++j)
			{
				ply->DelPai(ply->m_vecSanZhang[j]);
			}
			sendPro.set_direct(huanType);
			sendPro.set_seat(plyNext->GetSeat());
			sendPro.set_type(getType);
			plyNext->SendMsgToClient(::comdef::msg_maj, ::msg_maj::huan_sanzhang_notify, sendPro);
		}
	}
	else if (huanType == 1)
	{
		// 顺时针
		for (int i = 0; i < 4; ++i)
		{
			::msg_maj::HuanSanZhangNotify sendPro;
			uint16_t getType = 0;
			CPlayer* ply = m_vecPlayer[i];
			CPlayer* plyNext = m_vecPlayer[(i + 3) % 4];
			for (int j = 0; j < ply->m_vecSanZhang.size(); ++j)
			{
				getType = ply->m_vecSanZhang[j] / 10;
				plyNext->AddPai(ply->m_vecSanZhang[j]);
				sendPro.add_pais(ply->m_vecSanZhang[j]);
			}
			for (int j = 0; j < ply->m_vecSanZhang.size(); ++j)
			{
				ply->DelPai(ply->m_vecSanZhang[j]);
			}
			sendPro.set_direct(huanType);
			sendPro.set_seat(plyNext->GetSeat());
			sendPro.set_type(getType);
			plyNext->SendMsgToClient(::comdef::msg_maj, ::msg_maj::huan_sanzhang_notify, sendPro);
		}
	}
	else
	{
		// 对调
		for (int i = 0; i < 4; ++i)
		{
			::msg_maj::HuanSanZhangNotify sendPro;
			uint16_t getType = 0;
			CPlayer* ply = m_vecPlayer[i];
			CPlayer* plyNext = m_vecPlayer[(i + 2) % 4];
			for (int j = 0; j < ply->m_vecSanZhang.size(); ++j)
			{
				getType = ply->m_vecSanZhang[j] / 10;
				plyNext->AddPai(ply->m_vecSanZhang[j]);
				sendPro.add_pais(ply->m_vecSanZhang[j]);
			}
			for (int j = 0; j < ply->m_vecSanZhang.size(); ++j)
			{
				ply->DelPai(ply->m_vecSanZhang[j]);
			}
			sendPro.set_direct(huanType);
			sendPro.set_seat(plyNext->GetSeat());
			sendPro.set_type(getType);
			plyNext->SendMsgToClient(::comdef::msg_maj, ::msg_maj::huan_sanzhang_notify, sendPro);
		}
	}
}

void CRoom::StartTimerSendCard()
{
	m_pTimerSendCard->start(1 * 100, boost::bind(&CRoom::SendCard, this), fogs::FogsTimer::SINGLE_SHOOT_TIMER);
}

void CRoom::StartTimerClose()
{
	uint32_t unTime = 3600;
	if (unTime == 0)
	{
		unTime = 1;
	}
	m_pTimerClose->cancel();
	m_pTimerClose->start(unTime * 3600 * 1000, boost::bind(&CRoom::Close, this, eRoomCloses_TimeOut ), fogs::FogsTimer::SINGLE_SHOOT_TIMER);
}

void CRoom::CancelTimerClose()
{
	m_pTimerClose->cancel();
}

void CRoom::Close(int32_t reason)
{
	// 检查是否要保存当前的记录
	if (m_bStart)
	{
		if (m_pMaj->m_bEnd == false)
		{
			if (reason != eRoomCloses_Finish)
			{
				RecordInnZhanJiLocal(true);
				SaveInnZhangJiGamedb();
			}
		}

		if (m_unRecordID && !m_mapInnRecords.empty())
		{
			fogs::proto::msg::ZhanJiFinish histFinishProto;
			histFinishProto.set_record_id(m_unRecordID);
			GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::ZhanJiFinishRequestID, histFinishProto);
		}
	}

	m_bClose = true;

	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		CPlayer* pPlayer = m_vecPlayer[i];
		if (NULL == pPlayer)
		{
			continue;
		}

		SetLeaveData(i);

		if (pPlayer->IsDisconnect())
		{
			CWorld::Instance()->LeaveGame(pPlayer->GetSessionID());
		}
		else
		{
			pPlayer->SyncToWs();
		}
	}

	CRoomMgr::Instance()->AddCloseRoom(m_unRoomID);

}

void CRoom::SendCard()
{
	CPlayer* pPlayer = GetPlayer(m_pMaj->GetSendCardPos());
	if (NULL == pPlayer )
	{
		//LOG(ERROR) << "CRoom::SendCard() pos: " << m_pMaj->GetSendCardPos();
		return;
	}

	if (!m_pRule->HuedCanSendCard(pPlayer))
	{
		m_pMaj->SetSendCardPos(m_pMaj->GetSendCardPos() + 1);
		SendCard();
		return;
	}

	if (!pPlayer->CheckPaiNum())
	{
		//LOG(ERROR) << "CRoom::SendCard() pos: " << m_pMaj->GetSendCardPos();
		return;
	}

	if (m_pRule->IsGuoHu())
	{
		pPlayer->m_usGuohuPai = 0;
	}

	if (m_pRule->IsGuoPeng())
	{
		pPlayer->m_setGuoPengPai.clear();
	}

	std::vector<uint16_t> copy_pai_list = pPlayer->GetPaiList();

	m_pMaj->m_eActionType = eActionType_SendCard;
	m_pMaj->m_usCurActionPos = m_pMaj->GetSendCardPos();
	m_pMaj->m_usCurActionPai = m_pMaj->GetPai();
	m_pMaj->m_bActionEvent = false;
	m_pMaj->m_usDicardPos = m_pMaj->GetSendCardPos();
	++m_pMaj->m_usSendCardNum;
	pPlayer->AddPai(m_pMaj->m_usCurActionPai);

	::msg_maj::DealNotify proData;
	proData.set_seat(m_pMaj->m_usCurActionPos);
	proData.set_tile(m_pMaj->m_usCurActionPai);
	proData.set_tail(false);
	proData.set_desk_tile_count(m_pMaj->GetRemainPaiNum());

	// 记录跑的位置数量
	m_pMaj->SetLoopPoses(m_pMaj->GetLoopPoses() + 1);

	//检测胡牌
	stHuPai sthupai;
	sthupai.m_eHupaiType = m_pRule->CheckHupaiAll(pPlayer, pPlayer->GetPaiList(), pPlayer->GetEventPaiList());
	if (sthupai.m_eHupaiType != ::msg_maj::hu_none) //胡牌
	{
		if (m_pMaj->m_usGang > 0)
		{
			sthupai.m_eHupaiWay = m_pRule->GetHuWayGangKaiHua();
		}
		else
		{
			sthupai.m_eHupaiWay = m_pRule->GetHuWayZiMo();
		}
		sthupai.m_usHupaiPos = m_pMaj->m_usCurActionPos;
		sthupai.m_usPai = m_pMaj->m_usCurActionPai;

		m_pMaj->AddHuPai(sthupai);
		m_pMaj->AddSeatEvent(::msg_maj::zi_mo_hu, m_pMaj->m_usCurActionPos);
		::msg_maj::EventInfo* eventInfo = proData.add_eventlist();
		if (eventInfo)
		{
			eventInfo->set_event_t(::msg_maj::zi_mo_hu);
			eventInfo->add_event_pai(m_pMaj->m_usCurActionPai);
		}
	}

	//检测暗杠
	if (m_pRule->HuedCanGang(pPlayer))
	{
		std::vector<uint16_t> agpailist;
		m_pRule->CheckAnGang(pPlayer->GetPaiList(), agpailist);
		if (agpailist.size() > 0)
		{
			// 鬼牌不能暗杠
			for (std::vector<uint16_t>::iterator it2 = agpailist.begin(); it2 != agpailist.end();)
			{
				if (!m_pRule->GhostCanGang() && m_pRule->IsGhostCard(*it2))
				{
					it2 = agpailist.erase(it2);
				}
				else if (m_pRule->IsFliterPaiEvent(pPlayer, *it2))
				{
					it2 = agpailist.erase(it2);
				}
				else
				{
					++it2;
				}
			}

			if (agpailist.size() > 0)
			{
				//最后一张牌处理
				if (m_pMaj->CheckHasPai())
				{
					if (pPlayer->IsHued()) // 杠后还必须是要听牌
					{
						for (std::vector<uint16_t>::iterator iter = agpailist.begin(); iter != agpailist.end(); )
						{
							if (!m_pRule->HuedCanGangThisPai(copy_pai_list, *iter))// 该杠不允许杠，会破坏听牌
								iter = agpailist.erase(iter); 
							else
								++iter;
						}
					}

					if (agpailist.size() > 0)
					{
						m_pMaj->AddSeatEvent(::msg_maj::an_gang, m_pMaj->m_usCurActionPos);
						::msg_maj::EventInfo* info = proData.add_eventlist();
						info->set_event_t(::msg_maj::an_gang);
						for (std::vector<uint16_t>::iterator iter = agpailist.begin(); iter != agpailist.end(); ++iter)
						{
							info->add_event_pai(*iter);
						}
					}
				}
			}
		}

		//检测过手杠
		::msg_maj::EventInfo* info = NULL;
		if (pPlayer->CheckNextGang(m_pMaj->m_usCurActionPai))
		{
			//最后一张牌处理
			if (m_pMaj->CheckHasPai() && !pPlayer->HasGuoGangPai(m_pMaj->m_usCurActionPai))
			{
				m_pMaj->AddSeatEvent(::msg_maj::guo_shou_gang, m_pMaj->m_usCurActionPos);
				info = proData.add_eventlist();
				info->set_event_t(::msg_maj::guo_shou_gang);
				info->add_event_pai(m_pMaj->m_usCurActionPai);
			}
		}

		// 检查补杠
		std::vector<uint16_t> bgpailist;
		if (m_pRule->CanBuGang(pPlayer, bgpailist))
		{
			// 鬼牌不能暗杠
			for (std::vector<uint16_t>::iterator it2 = bgpailist.begin(); it2 != bgpailist.end();)
			{
				if (m_pRule->IsGhostCard(*it2))
				{
					it2 = bgpailist.erase(it2);
				}
				else if (m_pRule->IsFliterPaiEvent(pPlayer, *it2))
				{
					it2 = bgpailist.erase(it2);
				}
				else
				{
					++it2;
				}
			}

			if (bgpailist.size() > 0)
			{
				//最后一张牌处理
				if (m_pMaj->CheckHasPai())
				{
					for (std::vector<uint16_t>::iterator iter = bgpailist.begin(); iter != bgpailist.end(); ++iter)
					{
						if (m_pMaj->m_usCurActionPai != *iter)
						{
							if (info == NULL)
							{
								info = proData.add_eventlist();
								info->set_event_t(::msg_maj::guo_shou_gang);
								m_pMaj->AddSeatEvent(::msg_maj::guo_shou_gang, m_pMaj->m_usCurActionPos);
							}
							info->add_event_pai(*iter);
						}
					}

				}
			}
		}
	}

	BroadcastSendCard(proData);

	// 听牌
	this->TingPaiDiscardPai(pPlayer);

	//发牌记录
	stReplayAction st;
	st.event_t = ::msg_maj::deal;
	st.actor_seat = m_pMaj->m_usCurActionPos;
	st.event_tile_list.push_back(m_pMaj->m_usCurActionPai);
	st.desk_tile_count = m_pMaj->GetRemainPaiNum();
	AddReplayAction(m_nAllGames - m_nGames + 1, st);

	if (m_pRule->IsGhost() && !m_pMaj->m_bHadSendGhost)
	{
		m_pMaj->m_bHadSendGhost = true;
		SendNotifyGhostPaiListResult(NULL,false);
	}
}

void CRoom::BroadcastSendCard(::msg_maj::DealNotify proData)
{
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i] || m_vecPlayer[i]->IsDisconnect())
		{
			continue;
		}

		if (!m_vecPlayer[i]->IsEnterRoomReady() )
		{
			continue;
		}

		if (!m_vecPlayer[i]->IsReconectOtherReady())
		{
			m_vecPlayer[i]->AddEnterRoomOtherMsg(::comdef::msg_maj, ::msg_maj::deal_tile_notify, proData);
			continue;
		}

		if (i == m_pMaj->m_usDicardPos)
		{
			proData.set_tile(m_pMaj->m_usCurActionPai);
		}
		else
		{
			proData.set_tile(0);
		}

		m_vecPlayer[i]->SendMsgToClient(::comdef::msg_maj, ::msg_maj::deal_tile_notify, proData);
	}
}

void CRoom::StartRobotJoin()
{
	// 检查本地是否有空闲的机器人
	CPlayer* pRobot = CWorld::Instance()->GetFreeRobot();
	if (pRobot)
	{
		pRobot->JoinRoom(GetRoomID());
	}
	else
	{
		::msg_maj::ReqRobotJoinRoom send;
		send.set_room_id(m_unRoomID);
		GameService::Instance()->SendToWs(::comdef::msg_ss, ::msg_maj::req_robot_join_room, send);
	}
}

bool CRoom::EnterPlayer(CPlayer* pPlayer)
{
	if (NULL == pPlayer)
	{
		return false;
	}

	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i])
		{
			m_vecPlayer[i] = pPlayer;
			pPlayer->SetRoomID(m_unRoomID);
			pPlayer->SetSeat(i);
			return true;
		}
	}

	return false;
}

void CRoom::LeavePlayer(CPlayer* pPlayer)
{
	if (NULL == pPlayer)
	{
		return;
	}

	if (m_bStart)
	{
		this->DissmissRoomReq(pPlayer);
		return;
	}

	if (m_eRoomStatus != eRoomStatus_Ready)
	{
		//LOG(ERROR) << "CRoom::LeavePlayer()";
		return;
	}

	CPlayer* pPlayer_room = GetPlayer(pPlayer->GetSeat());
	if (NULL == pPlayer_room || pPlayer_room != pPlayer)
	{
		//LOG(ERROR) << "CRoom::LeavePlayer()";
		//做个保护处理
		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (m_vecPlayer[i] == pPlayer)
			{
				::msg_maj::RoleLeaveRoomNotify proBrocast; //通知其他人离开房间
				proBrocast.set_seat(i);
				proBrocast.set_is_roomer(false);
				BrocastMsg(::comdef::msg_room, ::msg_maj::role_leave_room, proBrocast);

				SetLeaveData(i);
				break;
			}
		}
		return;
	}

	if (pPlayer->GetCharID() == m_ulRoomer)
	{
		::msg_maj::RoleLeaveRoomNotify proBrocast; //通知其他人离开房间
		proBrocast.set_seat(pPlayer->GetSeat());
		proBrocast.set_is_roomer(true);
		BrocastMsg(::comdef::msg_room, ::msg_maj::role_leave_room, proBrocast);

		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (m_vecPlayer[i])
			{
				SetLeaveData(i);
			}
		}
	}
	else
	{
		::msg_maj::RoleLeaveRoomNotify proBrocast; //通知其他人离开房间
		proBrocast.set_seat(pPlayer_room->GetSeat());
		proBrocast.set_is_roomer(false);
		BrocastMsg(::comdef::msg_room, ::msg_maj::role_leave_room, proBrocast);

		SetLeaveData(pPlayer_room->GetSeat());
	}

	if (GetCurPersons() == 0)
	{
		Close(eRoomCloses_NotPersons);
	}
}

void CRoom::SetLeaveData(uint16_t usSeat)
{
	if (m_vecPlayer[usSeat])
	{
		m_vecPlayer[usSeat]->LeaveRoomEx();
		m_vecPlayer[usSeat] = NULL;
	}
}

void CRoom::RoomStatusEvent(CPlayer* pPlayer, bool bDisconnect)
{
	switch (m_eRoomStatus)
	{
	case eRoomStatus_Ready:
	{
		if (!bDisconnect && !m_bStart && CheckStart())
		{
			SendStartButton();
			break;
		}

		if (bDisconnect && !m_pMaj->m_bEnd && pPlayer->IsDisconnect())
		{
			SendCancelStartButton();
			break;
		}

		if (m_bStart &&!bDisconnect && !pPlayer->IsPrepare())
		{
			this->PreparRoundCheck(pPlayer);
		}
		break;
	}
	case eRoomStatus_ClickStart:
	{
		//同意开始
		if (bDisconnect && !pPlayer->IsAccept())
		{
			AcceptAskReq(pPlayer, false);
		}
		break;
	}
	case eRoomStatus_SanZhang:
	{
		if (!bDisconnect && !pPlayer->IsSanZhanged())
		{
			SendAskSanZhang(NULL);
		}
		break;
	}
	case eRoomStatus_DingQue:
	{
		if (!bDisconnect && !pPlayer->IsDingQueed())
		{
			SendAskDingQue(NULL);
		}
		break;
	}
	case eRoomStatus_StartGame:
	{
		//发牌
		if (!pPlayer->IsDisoverCard())
		{
			pPlayer->SetDisoverCard(true);

			if (DisoverCardAll())
			{
				SendCard();
			}
		}

		//解散
		if (m_bClickDissmiss &&
			bDisconnect &&
			!pPlayer->IsDismissAccept())
		{
			m_bClickDissmiss = false;

			for (uint16_t i = 0; i < m_usRoomPersons; ++i)
			{
				if (m_vecPlayer[i])
				{
					m_vecPlayer[i]->SetDismissAccept(false);
				}
			}

			::msg_maj::AgreeDismissResp proRep;
			proRep.set_seat(pPlayer->GetSeat());
			proRep.set_isagree(false);
			BrocastMsg(::comdef::msg_room, ::msg_maj::dismiss_room_vote_notify, proRep);
		}

		break;
	}
	case eRoomStatus_End:
	{
		break;
	}
	default:
		break;
	}
}

void CRoom::ReconnectPlayer(CPlayer* pPlayer)
{
	if (NULL == pPlayer)
	{
		return;
	}

	CancelTimerClose();

	//房间
	::msg_maj::ReconnectResp proRoom;
	proRoom.set_code(::msg_maj::ReconnectResp::SUCCESS);
	SetToProto(proRoom.mutable_room_info());
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i])
		{
			m_vecPlayer[i]->SetRoleInfoProto(proRoom.add_user_list());
		}
	}
	proRoom.set_self_seat(pPlayer->GetSeat());
	proRoom.set_banker_seat(m_usBankerSeat);
	if (pPlayer->GetCharID() == m_ulRoomer)
	{
		proRoom.set_is_roomer(true);
	}
	else
	{
		proRoom.set_is_roomer(false);
	}
	proRoom.set_is_start(m_bStart);
	pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::room_reconnect_resp, proRoom);

	//麻将
	::msg_maj::ReconnectLoadResp proMaj;
	GetReconnResp(pPlayer,proMaj);
	pPlayer->SendMsgToClient(::comdef::msg_maj, ::msg_maj::reconnect_resp, proMaj);

	::msg_maj::SyncPlayerStatus proData;
	proData.set_seat(pPlayer->GetSeat());
	proData.set_pstatus(::msg_maj::reconnect);
	BrocastMsg(::comdef::msg_maj, ::msg_maj::sync_player_status, proData, pPlayer);	
}

void CRoom::DisconnectPlayer(CPlayer* pPlayer)
{
	if (NULL == pPlayer)
	{
		return;
	}

	if (IsDisconnectAll())
	{
		StartTimerClose();
	}

	::msg_maj::SyncPlayerStatus proData;
	proData.set_seat(pPlayer->GetSeat());
	proData.set_pstatus(::msg_maj::disconnect);
	BrocastMsg(::comdef::msg_maj, ::msg_maj::sync_player_status, proData, pPlayer);

	RoomStatusEvent(pPlayer, true);
}

bool CRoom::IsDisconnectAll()
{
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i] && !m_vecPlayer[i]->IsDisconnect())
		{
			return false;
		}
	}

	return true;
}

CPlayer* CRoom::GetPlayer(uint16_t usSeat) const
{
	if (usSeat >= m_usRoomPersons)
	{
		//LOG(ERROR) << "CRoom::GetPlayer() usSeat: " << usSeat << " persons: " << m_usRoomPersons;
		return NULL;
	}

	return m_vecPlayer[usSeat];
}

uint16_t CRoom::GetCurPersons() const
{
	uint16_t usNum = 0;
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i])
		{
			++usNum;
		}
	}

	return usNum;
}

void CRoom::SetToProto(::msg_maj::RoomInfo* pPro)
{
	if (NULL == pPro)
	{
		return;
	}

	pPro->set_room_id(m_unRoomID);
	pPro->set_room_type(m_eRoomType);
	pPro->mutable_option()->CopyFrom(m_roomOption);
}

void CRoom::BrocastMsg(uint16_t usCmd, uint16_t usCCmd, const ::google::protobuf::Message& msg, CPlayer* pPlayer)
{
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i] || m_vecPlayer[i]->IsDisconnect())
		{
			continue;
		}

		if (pPlayer && pPlayer->GetCharID() == m_vecPlayer[i]->GetCharID())
		{
			continue;
		}

		if (!m_vecPlayer[i]->IsEnterRoomReady())
		{
			continue;
		}

		if (!m_vecPlayer[i]->IsReconectOtherReady())
		{
			m_vecPlayer[i]->AddEnterRoomOtherMsg(usCmd, usCCmd, msg);
			continue;
		}

		m_vecPlayer[i]->SendMsgToClient(usCmd, usCCmd, msg);
	}
}

void CRoom::SendRoomInnInfo(CPlayer* pPlayer)
{
	::msg_maj::SyncRommInnInfo proto;
	proto.set_inn_id(m_nAllGames - m_nGames + 1);
	if (pPlayer)
		pPlayer->SendMsgToClient(::comdef::msg_room,::msg_maj::sync_romm_inn_info, proto);
	else
		BrocastMsg(::comdef::msg_room, ::msg_maj::sync_romm_inn_info, proto, NULL);
}

void CRoom::AcceptAskReq(CPlayer* pPlayer, bool bAccept)
{
	if (!bAccept && m_eRoomStatus != eRoomStatus_ClickStart)
	{
		return;
	}

	if (pPlayer->IsAccept())
	{
		return;
	}

	pPlayer->SetAccept(bAccept);
	::msg_maj::AcceptStartNotify proRep;
	proRep.set_seat(pPlayer->GetSeat());
	proRep.set_accept(bAccept);
	BrocastMsg(::comdef::msg_maj, ::msg_maj::accept_start_notify, proRep);

	if (!bAccept)
	{
		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (m_vecPlayer[i])
			{
				m_vecPlayer[i]->SetAccept(false);
			}	
		}

		m_eRoomStatus = eRoomStatus_Ready;

		if (CheckStart())
		{
			SendStartButton();
		}

		return;
	}

	if (AcceptAskAll())
	{
		if (!CheckAndCostRoomCard())
		{
			for (uint16_t i = 0; i < m_usRoomPersons; ++i)
			{
				if (m_vecPlayer[i])
				{
					m_vecPlayer[i]->SetAccept(false);
				}
			}

			m_eRoomStatus = eRoomStatus_Ready;

			// 房上不足
			::msg_maj::OpenRoomResp proRep;
			proRep.set_code(::msg_maj::OpenRoomResp::ROOMCARD_NOTENOUTH);
			pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::open_room_resp, proRep);
			return;
		}

		m_bStart = true;
		m_startTime = time(NULL);

		//如果是还不能发牌的，则需要对些状态做判断处理，如选炮子
		//eRoomStatus status = m_pRule->AcceptAskAllNextStatus();
		
		SendHandCards();

		// 记录房间信息 todo 

		::fogs::proto::msg::RoomOptionCreate roomOption;

		roomOption.set_id(m_unRecordID);
		roomOption.set_create_time(m_reconrdTime);
		roomOption.set_start_time(m_startTime);
		roomOption.set_end_time(0);
		roomOption.set_gameid(GameService::Instance()->GetServerID());
		roomOption.set_room_no(m_unRoomID);
		roomOption.set_play_type(m_usGameType);
		roomOption.set_person_type(m_pRule->GetPlayerNum());
		roomOption.set_pay_type(m_pRule->GetPayType());
		roomOption.set_jushu_type(m_nAllGames);
		roomOption.set_score_type(m_pRule->GetBaseScore());
		roomOption.set_top_type(m_pRule->GetTopScore());
		roomOption.set_maima_type(m_pRule->GetMaiMaNum());
		roomOption.set_wanfaval(m_pRule->m_usWanFaVal);

		GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::SaveRoomOptionReqID, roomOption);
	}
}

// 选择定三张
void CRoom::SanZhangReq(CPlayer* pPlayer, std::vector<uint16_t>& vecSanZhang)
{
	if (m_eRoomStatus != eRoomStatus_SanZhang)
	{
		return;
	}

	if (pPlayer->IsSanZhanged())
	{
		return;
	}

	pPlayer->SetSanZhang(true);

	pPlayer->m_vecSanZhang.clear();
	for (size_t i = 0; i < vecSanZhang.size(); ++i)
	{
		pPlayer->m_vecSanZhang.push_back(vecSanZhang[i]);
	}

	if (SanZhangAll())
	{
		ExchangeSanZhangPai();
		m_eRoomStatus = eRoomStatus_DingQue;
		SendAskDingQue(NULL);
	}
}

void CRoom::SendDingQueNotify(CPlayer* pPlayer)
{
	// 广播所有选择的定缺
	::msg_maj::DingQueNotify sendNotify;
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		::msg_maj::DingQueReq* dingQue = sendNotify.add_quelist();
		dingQue->set_seat(i);
		dingQue->set_type(m_vecPlayer[i]->m_usDingQueType);
	}

	if (pPlayer)
	{
		pPlayer->SendMsgToClient(::comdef::msg_maj, ::msg_maj::dingque_notify, sendNotify);
	}
	else
	{
		BrocastMsg(::comdef::msg_maj, ::msg_maj::dingque_notify, sendNotify);
	}
}

// 选择定一门
void CRoom::DingQuiReq(CPlayer* pPlayer, uint16_t usLessType)
{
	if (m_eRoomStatus != eRoomStatus_DingQue)
	{
		return;
	}

	if (pPlayer->IsDingQueed())
	{
		return;
	}

	pPlayer->SetDingQue(true);
	pPlayer->m_usDingQueType = usLessType;
	
	if (DingQueAll())
	{
		SendDingQueNotify(NULL);

		if (m_nAllGames != m_nGames) // 第二局起
		{
			m_pMaj->m_bEnd = false;
		}
		else // 第一局
		{
			RecordPlayerInfo();
		}
		m_eRoomStatus = eRoomStatus_StartGame;
		SendCard();
		//m_usSanZhangStep = 1;
		m_bStart = true;
	}
}

bool CRoom::CheckAndCostRoomCard()
{
	CPlayer* hostPlayer = CWorld::Instance()->GetPlayerByUUID(m_ulRoomer);
	if (hostPlayer)
	{
		// 检查需要的房卡
		if (!CWorld::Instance()->IsFreeCard())
		{
			int32_t nNeedRoodCards = m_pRule->GetCostCard();
			if (m_pRule->GetPayType() == 1)
			{
				for (int i = 0; i < m_vecPlayer.size(); ++i)
				{
					if (m_vecPlayer[i])
					{
						if (m_vecPlayer[i]->GetRoomCard() < nNeedRoodCards)
						{
							return false;
						}
					}
				}
				for (int i = 0; i <  m_vecPlayer.size(); ++i)
				{
					if (!m_vecPlayer[i]->SubRoomCards(nNeedRoodCards, ::fogs::proto::msg::log_t_roomcard_sub_startmatch))
					{
						return false;	// 异常情况，房卡中途被消费
					}
					m_vecPlayer[i]->SendRoomCards();
				}
			}
			else
			{
				if (!hostPlayer->SubRoomCards(nNeedRoodCards, ::fogs::proto::msg::log_t_roomcard_sub_startmatch))
				{
					return false;	// 异常情况，房卡中途被消费
				}
			}
			hostPlayer->SendRoomCards();
		}
	}
	return true;
}

void CRoom::RecordPlayerInfo()
{
	//开局记录玩家数据
	std::vector<stPlayerInfo> playerinfolist;
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i])
		{
			continue;
		}

		stPlayerInfo st;
		st.uid = m_vecPlayer[i]->GetCharID();
		st.seat = i;
		st.nick = m_vecPlayer[i]->GetCharName();
		st.actor_addr = m_vecPlayer[i]->GetLogoIcon();
		playerinfolist.push_back(st);
	}
	AddPlayerInfo(playerinfolist);
}

bool CRoom::AcceptAskAll()
{
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i] && !m_vecPlayer[i]->IsAccept())
		{
			return false;
		}
	}

	return true;
}

bool CRoom::SanZhangAll()
{
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i] && !m_vecPlayer[i]->IsSanZhanged())
		{
			return false;
		}
	}
	return true;
}

bool CRoom::DingQueAll()
{
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i] && !m_vecPlayer[i]->IsDingQueed())
		{
			return false;
		}
	}
	return true;
}

void CRoom::DissmissRoomReq(CPlayer* pPlayer)
{
	if (m_eRoomStatus == eRoomStatus_Ready && pPlayer->GetCharID() == m_ulRoomer && !m_pMaj->m_bEnd)
	{
		::msg_maj::DismissRoomResp proData;
		proData.set_code(::msg_maj::DismissRoomResp::SUCCESS);
		proData.set_isdissmis(true);
		BrocastMsg(::comdef::msg_room, ::msg_maj::dismiss_room_resp, proData);

		DissmissRoom(eRoomCloses_Hand);
		return;
	}

	uint32_t unRemainTime = GetDissmissRoomProtectTime();
	unRemainTime = 0;
	if (unRemainTime > 0)
	{
		::msg_maj::DismissRoomResp proRep;
		proRep.set_code(::msg_maj::DismissRoomResp::PROTECTION_TIME);
		proRep.set_remain_time(unRemainTime);
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::dismiss_room_resp, proRep);
		return;
	}

	//pPlayer->SetDismissAccept(true);
	m_bClickDissmiss = true;

	::msg_maj::DismissRoomNotify proData;
	proData.set_nickname(pPlayer->GetCharName());
	proData.set_seat(pPlayer->GetSeat());
	proData.set_timer_sec(30);
	BrocastMsg(::comdef::msg_room, ::msg_maj::dismiss_room_notify, proData, NULL);
}

void CRoom::DissmissAcceptReq(CPlayer* pPlayer, bool bAccept)
{
	if (!m_bClickDissmiss || pPlayer->IsDismissAccept())
	{
		return;
	}

	pPlayer->SetDismissAccept(bAccept);

	::msg_maj::AgreeDismissResp proRep;
	proRep.set_seat(pPlayer->GetSeat());
	proRep.set_isagree(bAccept);	

	::msg_maj::DismissRoomResp proData;
	proData.set_code(::msg_maj::DismissRoomResp::SUCCESS);
	if (!bAccept)
	{
		m_bClickDissmiss = false;
		proData.set_isdissmis(false);
		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (m_vecPlayer[i])
			{
				m_vecPlayer[i]->SetDismissAccept(false);
			}	
		}
		BrocastMsg(::comdef::msg_room, ::msg_maj::dismiss_room_vote_notify, proRep);
		BrocastMsg(::comdef::msg_room, ::msg_maj::dismiss_room_resp, proData);
		return;
	}

	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i])
		{
			::msg_maj::DismissStatus* pPro = proRep.add_dismiss_list();
			pPro->set_seat(i);
			pPro->set_isagree(m_vecPlayer[i]->IsDismissAccept());
		}
	}

	BrocastMsg(::comdef::msg_room, ::msg_maj::dismiss_room_vote_notify, proRep);
	
	if (DissmissAcceptAll())
	{
		m_bClickDissmiss = false;

		for (uint16_t i = 0; i < this->GetTotalPersons(); ++i)
		{
			this->GetPlayer(i)->UpdateTotalFanAll();
		}

		EndAll();

		proData.set_isdissmis(true);
		BrocastMsg(::comdef::msg_room, ::msg_maj::dismiss_room_resp, proData);
		DissmissRoom(eRoomCloses_Hand);
	}
}

void CRoom::DissmissRoom(int32_t reason)
{
	Close(reason);
}

void CRoom::DelDiscardTile()
{
	CPlayer* pPlayer = GetPlayer(m_pMaj->m_usCurActionPos);
	if (pPlayer)
	{
		pPlayer->DelDiscardPai(m_pMaj->m_usCurActionPai);
	}
}

void CRoom::CountEventGang(uint16_t usSeat, uint16_t usEventType)
{
	m_pRule->CountEventGang(usSeat, usEventType);
}

bool CRoom::DissmissAcceptAll()
{
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i] || m_vecPlayer[i]->IsDisconnect())
		{
			continue;
		}
		if ( !m_vecPlayer[i]->IsDismissAccept())
		{
			return false;
		}
	}

	return true;
}

void CRoom::DisoverCardReq(CPlayer* pPlayer)
{
	if (pPlayer->IsDisoverCard())
	{
		return;
	}

	pPlayer->SetDisoverCard(true);

	if (DisoverCardAll())
	{
		eRoomStatus status = m_pRule->DisoverCardAllCheckAndDoEvent();
		switch (status)
		{
		case eRoomStatus_SanZhang:SendAskSanZhang(); break;
		case eRoomStatus_DingQue:SendAskDingQue(); break;
		default:SendCard(); break;
		}
	}
}

void CRoom::DiscardTileReq(CPlayer* pPlayer, uint16_t usPai)
{
	if (pPlayer->GetSeat() != m_pMaj->m_usDicardPos)
	{
		//LOG(ERROR) << "CRoom::DiscardTile() seat: " << pPlayer->GetSeat() << ", " << m_pMaj->m_usDicardPos;
		return;
	}

	if (!pPlayer->DiscardTile(usPai))
	{
		//LOG(ERROR) << "CRoom::DiscardTile() pai: " << usPai;
		return;
	}

	if (!m_pRule->HuedCanDiscard(pPlayer))
	{
		//LOG(ERROR) << "CRoom::HuedCanDiscard() pai: " << usPai;
		return;
	}

	m_pRule->CountGengZhuang(usPai);

	m_pMaj->m_eActionType = eActionType_Discard;
	m_pMaj->m_usCurActionPai = usPai;
	m_pMaj->m_usCurActionPos = pPlayer->GetSeat();
	
	m_pMaj->SetSendCardPos(pPlayer->GetSeat() + 1);
	m_pMaj->AddOutPai(usPai, 1);

	//事件检测
	bool bStop = false;
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (i == pPlayer->GetSeat() || NULL == m_vecPlayer[i] || !m_vecPlayer[i]->IsEnterRoomReady())
		{
			continue;
		}

		::msg_maj::DiscardResp proData;
		proData.set_seat(pPlayer->GetSeat());
		proData.set_tile(usPai);
		proData.set_tileleftcount(m_pMaj->GetRemainPaiNum());

		if (!m_pRule->HuedCanEvent(m_vecPlayer[i]))
		{
			m_vecPlayer[i]->SendMsgToClient(::comdef::msg_maj, ::msg_maj::discard_tile_resp, proData);
			continue;
		}

		if (m_pRule->IsFliterPaiEvent(m_vecPlayer[i], usPai))
		{
			m_vecPlayer[i]->SendMsgToClient(::comdef::msg_maj, ::msg_maj::discard_tile_resp, proData);
			continue;
		}

		if (m_pRule->IsGhost() && m_pRule->IsGhostCard(usPai))
		{
			m_vecPlayer[i]->SendMsgToClient(::comdef::msg_maj, ::msg_maj::discard_tile_resp, proData);
			continue;
		}

		bool bMyStop = false;

		bool isCanCheckPaoHu = true;

		// 如果有过胡功能的，要判断是否在过胡中，过胡中是不能点炮的
		if (m_pRule->IsGuoHu())
		{
			if (m_vecPlayer[i]->m_usGuohuPai > 0)
			{
				isCanCheckPaoHu = false;
			}
		}

		// 杠上炮
		if (isCanCheckPaoHu && m_pRule->HuedCanGangShanPao(m_vecPlayer[i]) && m_pMaj->m_usGang > 0)
		{
			std::vector<uint16_t> pailist_copy;
			GetCopyPaiList(pailist_copy, m_vecPlayer[i], usPai);

			stHuPai sthupai;
			sthupai.m_eHupaiType = m_pRule->CheckHupaiAll(m_vecPlayer[i], pailist_copy, m_vecPlayer[i]->GetEventPaiList());
				
			if (sthupai.m_eHupaiType != ::msg_maj::hu_none) //胡牌
			{
				bMyStop = true;
				sthupai.m_eHupaiWay = m_pRule->GetHuWayGangShanPao();
				sthupai.m_usHupaiPos = i;
				sthupai.m_usPai = usPai;
				m_pMaj->AddHuPai(sthupai);
				::msg_maj::EventInfo* info = proData.add_eventlist();
				info->set_event_t(::msg_maj::dian_pao_hu);
				info->add_event_pai((uint32_t)usPai);
				m_pMaj->AddSeatEvent(::msg_maj::dian_pao_hu, i);

				//LOG(ERROR) << "1111";
			}
		}

		// 点炮胡
		if (m_pRule->HuedCanDiaoPao(m_vecPlayer[i]) && !bMyStop && m_pRule->CanDianPao(m_vecPlayer[i]))
		{
			std::vector<uint16_t> pailist_copy;
			GetCopyPaiList(pailist_copy, m_vecPlayer[i],usPai);

			stHuPai sthupai;
			sthupai.m_eHupaiType = m_pRule->CheckHupaiAll(m_vecPlayer[i], pailist_copy, m_vecPlayer[i]->GetEventPaiList());
			if (sthupai.m_eHupaiType != ::msg_maj::hu_none) //胡牌
			{
				bStop = true;
				sthupai.m_eHupaiWay = m_pRule->GetHuWayDiaoPao();

				sthupai.m_usHupaiPos = i;
				sthupai.m_usPai = usPai;
				m_pMaj->AddHuPai(sthupai);

				::msg_maj::EventInfo* info = proData.add_eventlist();
				info->set_event_t(::msg_maj::dian_pao_hu);
				info->add_event_pai((uint32_t)usPai);

				m_pMaj->AddSeatEvent(::msg_maj::dian_pao_hu, i);

				//LOG(ERROR) << "2222";
			}
		}

		if (m_pMaj->CheckHasPai())
		{
			if (m_pRule->HuedCanGang(m_vecPlayer[i]))
			{
				if (!m_pRule->CheckGangPai(m_vecPlayer[i]->GetPaiList(), usPai))
				{
					if (m_pRule->HuedCanPeng(m_vecPlayer[i]) && m_pRule->CheckPengPai(m_vecPlayer[i]->GetPaiList(), usPai) && !m_pRule->IsGuoPengThisPai(m_vecPlayer[i], usPai))
					{
						bMyStop = true;
						m_pMaj->AddSeatEvent(::msg_maj::pong, i);
						::msg_maj::EventInfo* info = proData.add_eventlist();
						info->set_event_t(::msg_maj::pong);
						info->add_event_pai((uint32_t)usPai);

						//LOG(ERROR) << "33333";
					}
				}
				else
				{
					if (!m_vecPlayer[i]->IsHued() || m_pRule->HuedCanGangThisPai(m_vecPlayer[i]->GetPaiList(), usPai))// 该杠不允许杠，会破坏听牌
					{
						bMyStop = true;
						m_pMaj->AddSeatEvent(::msg_maj::ming_gang, i);

						if (m_pRule->HuedCanPeng(m_vecPlayer[i]))
						{
							m_pMaj->AddSeatEvent(::msg_maj::pong, i);
						}

						::msg_maj::EventInfo* info = proData.add_eventlist();
						info->set_event_t(::msg_maj::ming_gang);
						info->add_event_pai((uint32_t)usPai);

						if (m_pRule->HuedCanPeng(m_vecPlayer[i]))
						{
							::msg_maj::EventInfo* info2 = proData.add_eventlist();
							info2->set_event_t(::msg_maj::pong);
							info2->add_event_pai((uint32_t)usPai);
						}
					}
					
				}
			}
		}

		bStop = bStop || bMyStop;

		if (!m_vecPlayer[i]->IsReconectOtherReady())
		{
			m_vecPlayer[i]->AddEnterRoomOtherMsg(::comdef::msg_maj, ::msg_maj::discard_tile_resp, proData);
			continue;
		}

		m_vecPlayer[i]->SendMsgToClient(::comdef::msg_maj, ::msg_maj::discard_tile_resp, proData);
	}

	m_pMaj->m_usGang = 0;

	//记录事件动作
	stReplayAction st;
	st.event_t = ::msg_maj::discard;
	st.actor_seat = pPlayer->GetSeat();
	st.victim_seat = -1;
	st.event_tile_list.push_back(usPai);
	AddReplayAction(m_nAllGames - m_nGames + 1, st);

	if (!bStop && !m_pMaj->CheckHasPai())
	{
		End();
		return;
	}

	m_pMaj->SetSendCardPos(pPlayer->GetSeat() + 1);

	if (!bStop)
	{
		StartTimerSendCard();
	}
	else
	{
		return;
	}
}

bool CRoom::DisoverCardAll()
{
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i] && !m_vecPlayer[i]->IsDisoverCard())
		{
			return false;
		}
	}
	return true;
}

void CRoom::PrepareRoundReq(CPlayer* pPlayer)
{
	if (m_eRoomStatus != eRoomStatus_Ready || !m_pMaj->m_bEnd)
	{
		return;
	}

	this->PreparRoundCheck(pPlayer);
}

void CRoom::PreparRoundCheck(CPlayer* pPlayer)
{
	pPlayer->SetPrepare(true);
	::msg_maj::PrepareRoundNotify pro;
	pro.set_seat(pPlayer->GetSeat());
	pro.set_prepare(pPlayer->IsPrepare());

	BrocastMsg(::comdef::msg_maj, ::msg_maj::prepare_round_notify, pro);

	if (!PrepareRoundAll())
	{
		return;
	}

	// 如有选炮子的则要判断
	//m_pRule->PrepareRoundAllCheckAndDoAskSame();

	SendHandCards();
	m_pMaj->m_bEnd = false;
}

void CRoom::ReconnectReadyReq(CPlayer* pPlayer)
{
	if (!pPlayer->IsDisconnect())
	{
		return;
	}
	
	pPlayer->SetDisconnect(false);

	if (pPlayer->GetRoomID() <= 0)
	{
		::msg_maj::NotifyRoomDismiss proData;
		pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::notify_room_dismiss, proData);
		return;
	}

	if (!pPlayer->IsEnterRoomReady())
	{
		EnterRoomReadyReq(pPlayer);
		return;
	}

	::msg_maj::ReconnectSyncCard proData;
	proData.set_desk_tile_count(m_pMaj->GetRemainPaiNum());
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (i == pPlayer->GetSeat() || NULL == m_vecPlayer[i])
		{
			continue;
		}
		m_vecPlayer[i]->SetSeatInfoProto(proData.add_seats(), true);
	}

	if (proData.seats_size() > 0)
	{
		pPlayer->SendMsgToClient(::comdef::msg_maj, ::msg_maj::reconnect_sync_card, proData);
	}

	::msg_maj::SyncPlayerStatus proSync;
	proSync.set_seat(pPlayer->GetSeat());
	proSync.set_pstatus(::msg_maj::normal);
	BrocastMsg(::comdef::msg_maj, ::msg_maj::sync_player_status, proSync, NULL);

	SendRoomInnInfo(pPlayer);

	RoomStatusEvent(pPlayer, false);

	m_pRule->ReconnectReadyReqCheckAndNotify(pPlayer);

	if (IsXieZhanLiu() && DingQueAll())
	{
		SendDingQueNotify(pPlayer);
	}

	if (m_pRule->IsGhost())
	{
		SendNotifyGhostPaiListResult(pPlayer, true);
	}

	if (m_pMaj->m_eActionType == eActionType_SendCard && m_pMaj->m_usCurActionPos == pPlayer->GetSeat())
	{
		TingPaiDiscardPai(pPlayer);
	}
	
}

void CRoom::ReconnectOtherReadyReq(CPlayer* pPlayer)
{
	if (!pPlayer->IsReconectOtherReady())
	{
		ReconectOtherReadyReq(pPlayer);
		return;
	}
}

void CRoom::KickRoleReq(CPlayer* pPlayer, uint16_t usSeat)
{
	if (pPlayer->GetCharID() != m_ulRoomer)
	{
		//LOG(ERROR) << "not roomer can not kick role";
		return;
	}

	if (usSeat == pPlayer->GetSeat())
	{
		//LOG(ERROR) << "CRoom::KickRoleReq() seat: " << usSeat;
		return;
	}

	CPlayer* pPlayer_room = GetPlayer(usSeat);
	if (NULL == pPlayer_room)
	{
		//LOG(ERROR) << "CRoom::KickRoleReq() seat: " << usSeat;
		return;
	}

	::msg_maj::KickRoleNotify proData;
	proData.set_seat(pPlayer_room->GetSeat());
	BrocastMsg(::comdef::msg_room, ::msg_maj::kick_role_notify, proData);

	SetLeaveData(pPlayer_room->GetSeat());
}

void CRoom::EnterRoomReadyReq(CPlayer* pPlayer)
{
	if (pPlayer->IsEnterRoomReady())
	{
		return;
	}

	pPlayer->SetEnterRoomReady(true);

	::msg_maj::SyncRoomRoleInfo proData;
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (i == pPlayer->GetSeat() || 
			NULL == m_vecPlayer[i] || 
			!m_vecPlayer[i]->IsEnterRoomReady())
		{
			continue;
		}

		m_vecPlayer[i]->SetRoleInfoProto(proData.add_rolelist());
	}
	pPlayer->SendMsgToClient(::comdef::msg_room, ::msg_maj::sync_room_role_info, proData);

	::msg_maj::RoleEnterRoomNotify proBrocast;
	pPlayer->SetRoleInfoProto(proBrocast.mutable_role());
	BrocastMsg(::comdef::msg_room, ::msg_maj::role_enter_room, proBrocast, pPlayer);
	//DUMP_PROTO_MSG(proBrocast);
	if (CheckStart())
	{
		SendStartButton();
	}
	else
	{
		if (this->IsRobot())
		{
			this->StartRobotJoin();
		}
	}
}

void CRoom::ReconectOtherReadyReq(CPlayer* pPlayer)
{
	if (pPlayer->IsReconectOtherReady())
	{
		return;
	}
	pPlayer->SetReconectOtherReady(true);
	pPlayer->SendEnterRoomOtherMsg();
}

bool CRoom::PrepareRoundAll()
{
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i] && !m_vecPlayer[i]->IsPrepare())
		{
			return false;
		}
	}

	return true;
}

void CRoom::ResponseEventReq(CPlayer* pPlayer, uint16_t usEventType, uint16_t usParam)
{
	if (!m_pMaj->CheckEvent(::msg_maj::event_type(usEventType), pPlayer->GetSeat()))
	{
		//LOG(ERROR) << "CRoom::ResponseEvent() etype: " << usEventType << " param: " << usParam << " seat: " << pPlayer->GetSeat();
		return;
	}

	if (m_pMaj->GetEventSeatNum() >= 2)
	{
		HandleEventMultiple(pPlayer, usEventType, usParam);
	}
	else
	{
		HandleEventSingle(pPlayer, usEventType, usParam);
	}
}

bool CRoom::End()
{
	if (m_pRule->HasMaiMa())
	{
		m_pMaj->OpenMaPai();
	}
	else if (m_pRule->IsYiMaAllHit())
	{
		m_pMaj->OpenMaPai();
	}

	m_pRule->CountResult();

	if (m_pRule->IsCanCountGang())
	{
		m_pRule->CountGangResult();
	}
	bool isEndAll = false;
	bool isReadEnd = m_pRule->HuedCanEnd();
	if (isReadEnd)
	{
		if (this->GetHuPlayerCount() < 3) // 流局
		{
			m_pRule->ChaHuaZhu();
			m_pRule->ChaDaJiao();
		}
		if (m_eRoomType == msg_maj::ROOM_PRIVATE) // 私人房卡场
		{
			for (uint16_t i = 0; i < this->GetTotalPersons(); ++i)
			{
				this->GetPlayer(i)->UpdateTotalFanAll();
			}
		}
		else if (m_eRoomType == msg_maj::ROOM_GOLD) // 豆子场
		{
			for (uint16_t i = 0; i < this->GetTotalPersons(); ++i)
			{
				CPlayer* tmp = this->GetPlayer(i);
				if (tmp)
				{
					tmp->SetCoin(tmp->GetCoin() + tmp->GetTotalFan());
					int32_t needValue = m_pRule->GetBaseScore();
					if (tmp->GetCoin() < needValue) // 底于底分破产，游戏结束
					{
						isEndAll = true;
						::msg_maj::NtCoinNotenough sendNot;
						tmp->SendMsgToClient(::comdef::msg_room, ::msg_maj::coin_notenough_notity, sendNot);
					}
				}
			}
		}
	}

	// 删除牌的操作，方便前端显示用
	if (m_pMaj->GetHuWay() != ::msg_maj::hu_way_none)
	{
		if (m_pMaj->m_usFirstSeat == -1)
		{
			m_pMaj->m_usFirstSeat = m_pMaj->GetHuPaiFirst().m_usHupaiPos;
		}
		switch (m_pMaj->GetHuPaiFirst().getDefHuWay())
		{
		case ::msg_maj::hu_way_zimo:
		case ::msg_maj::hu_way_gangkaihua:
		{
			CPlayer* p = GetPlayer(m_pMaj->m_usCurActionPos);
			if (p)
			{
				p->DelPai(m_pMaj->m_usCurActionPai);
			}
		}
		default:
			break;
		}

		if (m_pMaj->GetHuWay() == m_pRule->GetHuWayQiangGangHu())
		{
			CPlayer* p = GetPlayer(m_pMaj->m_usCurActionPos);
			if (p)
			{
				p->DelPai(m_pMaj->m_nLastGangPai);
			}
		}
	}

	if (isReadEnd)
	{
		::msg_maj::GameResultNotify pro;
		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (NULL == m_vecPlayer[i]) continue;

			::msg_maj::GameResultSeat* pPro = pro.add_seats();
			pPro->set_seat(m_vecPlayer[i]->GetSeat());
			pPro->set_total_score(m_vecPlayer[i]->GetTotalFan());
			const std::vector<uint16_t>& pailist = m_vecPlayer[i]->GetPaiList();
			for (std::vector<uint16_t>::const_iterator iter = pailist.begin(); iter != pailist.end(); ++iter)
			{
				pPro->add_hand_tiles(*iter);
			}
			const vecEventPai& epailist = m_vecPlayer[i]->GetEventPaiList();
			for (vecEventPai::const_iterator iter = epailist.begin(); iter != epailist.end(); ++iter)
			{
				::msg_maj::OpenTile* pProOpenTile = pPro->add_open_tiles();
				pProOpenTile->set_type(::msg_maj::event_type((*iter).usEventType));
				uint16_t usEPaiNum = (*iter).usEventType == ::msg_maj::pong ? 3 : 4;
				for (uint16_t j = 0; j < usEPaiNum; ++j)
				{
					pProOpenTile->add_tile_list((*iter).usPai);
				}
			}
			pPro->set_an_gang(m_pRule->GetFanAnGang(m_vecPlayer[i]));
			pPro->set_ming_gang(m_pRule->GetFanMingGang(m_vecPlayer[i]));
			pPro->set_guo_shou_gang(m_pRule->GetFanNextGang(m_vecPlayer[i]));

			//胡牌数据
			stHuPai sthupai;
			m_pMaj->GetHuPai(i, sthupai);
			if (sthupai.m_eHupaiType != ::msg_maj::hu_none  && !m_pRule->IsMultiHues())
			{
				m_pRule->SetHuInfo(sthupai, i, pPro->mutable_hu_info(), true);
				if (m_pMaj->GetHuWay() == m_pRule->GetHuWayQiangGangHu())
				{
					pPro->set_hu_tile(m_pMaj->m_nLastGangPai);
				}
				else
				{
					pPro->set_hu_tile(m_pMaj->m_usCurActionPai);
				}
			}

			pPro->set_dingque(m_vecPlayer[i]->m_usDingQueType);
			pPro->set_game_type(m_usGameType);

			// 所有的胡牌数据
			for (int k = 0; k < m_vecPlayer[i]->m_vecScoreDetail.size(); ++k)
			{
				FillHuTimesToDetail(m_vecPlayer[i]->m_vecScoreDetail[k], pPro->add_score_detail());
			}
		}

		bool isEndAll = m_nGames > 1 ? false : true;
		pro.set_this_inn_id(m_nAllGames - m_nGames + 1);
		pro.set_is_end_all(isEndAll);
		BrocastMsg(::comdef::msg_maj, ::msg_maj::game_result_notify, pro);

		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (m_vecPlayer[i] && m_vecPlayer[i]->GetBeInvitationID() > 0)
			{
				m_vecPlayer[i]->SetPlayGames(m_vecPlayer[i]->GetPlayGames() + 1);
				if (m_vecPlayer[i]->GetPlayGames() == 8)
				{
					m_vecPlayer[i]->SetRecvRewardStatus(1);
				}
			}

			if (m_vecPlayer[i])
			{
				m_vecPlayer[i]->AddTodayPlayNum(1);
			}
		}

		// 处理战绩数据
		RecordInnZhanJiLocal();

		SaveInnZhangJiGamedb();

		ChangeBankerSeat();

		--m_nGames;

		if (m_nGames > 0 && !isEndAll)
		{
			m_pMaj->m_bEnd = true;
			m_eRoomStatus = eRoomStatus_Ready;
			ResetData();
		}
		else
		{
			m_eRoomStatus = eRoomStatus_End;
			EndAll();
			DissmissRoom(eRoomCloses_Finish);
		}

	}

	// 同步玩家结果数据 
	::msg_maj::SyncRoomRoleInfo proData;
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i])
		{
			continue;
		}
		m_vecPlayer[i]->SetRoleInfoProto(proData.add_rolelist());
	}
	BrocastMsg(::comdef::msg_room, ::msg_maj::sync_room_role_info, proData);

	if (isReadEnd)
	{
		// 重置玩家数据 
		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (m_vecPlayer[i])
			{
				m_vecPlayer[i]->ResetMajData();
			}
		}
	}

	return isReadEnd;
}

void CRoom::EndAll()
{
	m_eRoomStatus = eRoomStatus_End;

	uint16_t usBigWinSeat = 0;
	int32_t nBigWinScore = 0;
	for (uint16_t i = 0; i < GetTotalPersons(); ++i)
	{
		if (NULL == m_vecPlayer[i])
		{
			continue;
		}

		int32_t nTotalFanAll = m_vecPlayer[i]->GetTotalFanAll();
		if (nTotalFanAll > nBigWinScore)
		{
			usBigWinSeat = i;
			nBigWinScore = nTotalFanAll;
		}
	}

	CPlayer* pPlayer = GetPlayer(usBigWinSeat);
	if (NULL == pPlayer)
	{
		//LOG(ERROR) << "CRoom::EndAll()";
		ASSERT(0);
		return;
	}

	pPlayer->SetBigWin();

	::msg_maj::TotalResultNotify proData;
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i])
		{
			m_vecPlayer[i]->SetTotalResultProto(proData.add_seats());
			m_vecPlayer[i]->m_usLastGameEnd = time(NULL);
		}
	}
	//DUMP_PROTO_MSG(proData);
	BrocastMsg(::comdef::msg_maj, ::msg_maj::total_result_notify, proData);

	// 更新时间
	::fogs::proto::msg::RoomOptionEnd roomOption;
	roomOption.set_id(m_unRecordID);
	roomOption.set_end_time(time(NULL));
	GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::UpdateRoomOptionReqID, roomOption);
}

uint16_t CRoom::GetHuPlayerCount() const
{
	uint16_t usTimes = 0;
	for (int i = 0; i < m_usRoomPersons; ++i)
	{
		if (m_vecPlayer[i]->IsHued())
		{
			usTimes++;
		}
	}
	return usTimes;
}

void CRoom::ResetData()
{
	m_eRoomStatus = eRoomStatus_Ready;
	//m_usSanZhangStep = -1;
	m_pMaj->Release();
}

void CRoom::ChangeBankerSeat()
{
	m_usBankerSeat = m_pRule->GetBankerSeat();

	if (m_usBankerSeat >= m_usRoomPersons)
	{
		//LOG(WARNING) << "CRoom::ChangeBankerSeat() seat: " << m_usBankerSeat << ", GameType:" << m_usGameType << ", Persons:" << m_usRoomPersons;
		m_usBankerSeat = 0;
	}

	m_pMaj->SetSendCardPos(m_usBankerSeat);
}

uint32_t CRoom::GetDissmissRoomProtectTime()
{
	uint32_t unTime = 0;
	time_t tCurTime = time(NULL);
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i] || !m_vecPlayer[i]->IsDisconnect())
		{
			continue;
		}

		if (tCurTime - m_vecPlayer[i]->GetDisconnectTime() >= 3600)
		{
			continue;
		}

		uint32_t unRemainTime = 3600 - (tCurTime - m_vecPlayer[i]->GetDisconnectTime());
		if (unRemainTime > unTime)
		{
			unTime = unRemainTime;
		}
	}

	return unTime;
}

void CRoom::HandleEventMultiple(CPlayer* pPlayer, uint16_t usEventType, uint16_t usParam)
{
	switch (usEventType)
	{
	case ::msg_maj::pass:
	{
		if (m_pRule->IsGuoPeng() && m_pMaj->HasThisEventTypeBySeat(pPlayer->GetSeat(), msg_maj::pong))
		{
			pPlayer->AddGuoPengPai(m_pMaj->m_usCurActionPai);
		}
		msg_maj::event_type eType = m_pMaj->GetEventTypeBySeat(pPlayer->GetSeat());
		stHuPai sthupai;
		m_pMaj->GetHuPai(pPlayer->GetSeat(), sthupai);
		switch (sthupai.getDefHuWay())
		{
		case ::msg_maj::hu_way_qiangganghu:
		case ::msg_maj::hu_way_gangshangpao:
		case ::msg_maj::hu_way_dianpao:
		{
			if (m_pRule->IsGuoHu())
			{
				pPlayer->m_usGuohuPai = 1;
			}
		}
		default:
			break;
		}
		m_pMaj->DelSeatEvent(pPlayer->GetSeat());
		m_pMaj->DelHuPai(pPlayer->GetSeat());
		break;
	}
	default:
	{
		if (!m_pMaj->DoSeatEvent(::msg_maj::event_type(usEventType), pPlayer->GetSeat()))
		{
			//LOG(ERROR) << "HandleEventMultiple(): " << usEventType << " " << pPlayer->GetSeat();
		}

		break;
	}
	}

	if (!m_pMaj->IsDoAllSeatEvent())
	{
		//LOG(ERROR) << "IsDoAllSeatEvent(): Not DoAll SeatEvent";
		return;
	}

	// 在end前保存一份备用
	vecSeatEvent vecSeatEvent = m_pMaj->m_vecSeatEvent; 

	if (m_pMaj->GetEventSeatNum() == 1)
	{
		stSeatEvent seatevent;
		m_pMaj->GetActionEvent(seatevent);
		CPlayer* p = GetPlayer(seatevent.usSeat);
		if (NULL == p)
		{
			//LOG(ERROR) << "HandleEventMultiple(): " << seatevent.usSeat;
			return;
		}

		if (m_pMaj->GetHuPaiFirst().getDefHuWay() == ::msg_maj::hu_way_qiangganghu && seatevent.etype == ::msg_maj::ming_gang)
		{
			if (p->MingGang(usParam))
			{
				p->MingGangSeated(m_pMaj->GetUsCurActionPos());
				CountEventGang(p->GetSeat(), ::msg_maj::ming_gang);
				DelDiscardTile();
				m_pMaj->m_bActionEvent = true;
				m_pMaj->SetSendCardPos(p->GetSeat());
				StartTimerSendCard();
				m_pMaj->m_usGang = 2;
			}
		}
		else
		{
			HandleEventSingle(p, seatevent.etype, usParam);
		}

		return;
	}

	//不需要
	//m_pMaj->AddOutPai(usParam, 1);

	::msg_maj::ResponseEventResp proRep;
	proRep.set_code(::msg_maj::ok);
	proRep.set_seat(pPlayer->GetSeat());
	proRep.set_event(::msg_maj::event_type(usEventType));
	proRep.set_victim_seat(m_pMaj->m_usCurActionPos);
	proRep.set_tileleftcount(m_pMaj->GetRemainPaiNum());

	//记录事件动作
	stReplayAction st;
	st.event_t = ::msg_maj::event_type(usEventType);
	st.actor_seat = pPlayer->GetSeat();
	st.victim_seat = -1;

	//多个胡牌

	const stHuPai& sthupai = m_pMaj->GetHuPaiFirst();
	if (sthupai.m_eHupaiType == ::msg_maj::hu_none)
	{
		//LOG(ERROR) << "CRoom::ResponseEventReq()";
		return;
	}

	if (sthupai.getDefHuWay() == ::msg_maj::hu_way_qiangganghu)
	{
		CPlayer* pGuo = GetPlayer(m_pMaj->GetSendCardPos());
		if (pGuo)
		{
			pGuo->DelEventPai(::msg_maj::guo_shou_gang, m_pMaj->m_nLastGangPai);
			pGuo->AddEventPai(::msg_maj::pong, m_pMaj->m_nLastGangPai);
		}
	}

	m_pRule->SetHuInfo(sthupai, pPlayer->GetSeat(), proRep.mutable_hu_info(), false);
	BrocastMsg(::comdef::msg_maj, ::msg_maj::response_resp, proRep);

	// 是否有截胡功能
	if (m_pRule->IsJieHu())
	{
		if (m_pMaj->GetVecHuPai().size() > 1)
		{
			uint16_t curPos = m_pMaj->GetUsCurActionPos();
			const vecHuPai& huPai = m_pMaj->GetVecHuPai();
			for (int i = 0; i < GetTotalPersons(); ++i)
			{
				bool foundIt = false;
				for (vecHuPai::const_iterator it = huPai.begin(); it != huPai.end(); ++it)
				{
					if (GetTotalPersons() > 0)
					{
						if (it->m_usHupaiPos == ((curPos + i) % GetTotalPersons()))
						{
							m_pMaj->DeleteHuPaiExtOne(it->m_usHupaiPos);
							foundIt = true;
							break;;
						}
					}
				}
				if (foundIt)
					break;
			}
		}
	}

	bool isReadEnd = End();

	m_pMaj->m_vecSeatEvent.clear();

	//游戏记录
	st.hu_info.hu_type = sthupai.m_eHupaiType;
	st.hu_info.hu_way = sthupai.m_eHupaiWay;
	for (std::vector<uint16_t>::const_iterator iter = m_pMaj->GetMaPaiList().begin(); iter != m_pMaj->GetMaPaiList().end(); ++iter)
	{
		st.hu_info.ma_pai_all.push_back(*iter);
	}

	for (std::vector<uint16_t>::const_iterator iter = pPlayer->GetHitMa().begin(); iter != pPlayer->GetHitMa().end(); ++iter)
	{
		st.hu_info.ma_pai_hit.push_back(*iter);
	}
	st.event_tile_list.push_back(usParam);
	AddReplayAction(m_nAllGames - m_nGames + 1, st);

	for (vecSeatEvent::const_iterator it = vecSeatEvent.begin(); it != vecSeatEvent.end(); ++it)
	{
		if (it->etype == ::msg_maj::zi_mo_hu || it->etype == ::msg_maj::dian_pao_hu)
		{
			::msg_maj::HuPaiListNotify sendHuList;
			sendHuList.set_seat(it->usSeat);
			sendHuList.add_tiles(usParam);
			BrocastMsg(::comdef::msg_maj, ::msg_maj::hu_pailist_notify, sendHuList);
		}
	}

	if (!isReadEnd)
	{
		m_pMaj->SetSendCardPos(m_pMaj->m_usCurActionPos + 1);
		StartTimerSendCard();
	}
}

void CRoom::HandleEventSingle(CPlayer* pPlayer, uint16_t usEventType, uint16_t usParam)
{
	::msg_maj::ResponseEventResp proRep;
	proRep.set_code(::msg_maj::ok);
	proRep.set_seat(pPlayer->GetSeat());
	proRep.set_event(::msg_maj::event_type(usEventType));
	proRep.set_victim_seat(m_pMaj->m_usCurActionPos);
	proRep.set_tileleftcount(m_pMaj->GetRemainPaiNum());

	//记录事件动作
	stReplayAction st;
	st.event_t = ::msg_maj::event_type(usEventType);
	st.actor_seat = pPlayer->GetSeat();
	st.victim_seat = -1;

	bool isReadEnd = false;

	switch (usEventType)
	{
	case ::msg_maj::pass:
	{
		if (m_pRule->IsGuoPeng() && m_pMaj->HasThisEventTypeBySeat(pPlayer->GetSeat(), msg_maj::pong))
		{
			pPlayer->AddGuoPengPai(m_pMaj->m_usCurActionPai);
		}
		::msg_maj::event_type eType = m_pMaj->GetEventTypeBySeat(pPlayer->GetSeat());
		stHuPai sthupai;
		m_pMaj->GetHuPai(pPlayer->GetSeat(), sthupai);
		switch (sthupai.getDefHuWay())
		{
		case ::msg_maj::hu_way_none:
		{
			switch (eType)
			{
			case msg_maj::an_gang:
			case msg_maj::guo_shou_gang:
			{
				break;
			}
			default:
			{
				StartTimerSendCard();
				break;
			}
			}
			break;
		}
		case ::msg_maj::hu_way_zimo:
		case ::msg_maj::hu_way_gangkaihua:
		{
			break;
		}
		case ::msg_maj::hu_way_dianpao:
		{
			if (m_pRule->IsGuoHu())
			{
				pPlayer->m_usGuohuPai = 1;
			}
			StartTimerSendCard();
			break;
		}
		case ::msg_maj::hu_way_qiangganghu:
		{
			CPlayer* pGuo = GetPlayer(m_pMaj->GetSendCardPos());
			if (pGuo)
			{
				CountEventGang(m_pMaj->GetSendCardPos(), ::msg_maj::guo_shou_gang);
				pGuo->AddNextGangTimes();
			}
			if (m_pRule->IsGuoHu())
			{
				pPlayer->m_usGuohuPai = 1;
			}
			StartTimerSendCard();
			break;
		}
		default:
		{
			StartTimerSendCard();
			break;
		}
		}

		m_pMaj->DelSeatEvent(pPlayer->GetSeat());
		m_pMaj->DelHuPai(pPlayer->GetSeat());
		break;
	}
	case ::msg_maj::zi_mo_hu:
	case ::msg_maj::dian_pao_hu:
	{
		stHuPai sthupai;
		m_pMaj->GetHuPai(pPlayer->GetSeat(), sthupai);
		if (sthupai.m_eHupaiType == ::msg_maj::hu_none)
		{
			//LOG(ERROR) << "CRoom::ResponseEventReq()";
			return;
		}
		
		switch (sthupai.getDefHuWay())
		{
		case ::msg_maj::hu_way_qiangganghu:
		{
			CPlayer* pGuo = GetPlayer(m_pMaj->GetSendCardPos());
			if (pGuo)
			{
				pGuo->DelEventPai(::msg_maj::guo_shou_gang, m_pMaj->m_nLastGangPai);
				pGuo->AddEventPai(::msg_maj::pong, m_pMaj->m_nLastGangPai);
			}
			break;
		}
		default:
			break;
		}

		if (usEventType == ::msg_maj::zi_mo_hu)
		{
			m_pMaj->AddOutPai(usParam, 1);
		}

		m_pRule->SetHuInfo(sthupai, pPlayer->GetSeat(), proRep.mutable_hu_info(), false);
		BrocastMsg(::comdef::msg_maj, ::msg_maj::response_resp, proRep);

		//游戏结束处理	
		isReadEnd = End();

		//游戏记录
		st.hu_info.hu_type = sthupai.m_eHupaiType;
		st.hu_info.hu_way = sthupai.m_eHupaiWay;
		for (std::vector<uint16_t>::const_iterator iter = m_pMaj->GetMaPaiList().begin(); iter != m_pMaj->GetMaPaiList().end(); ++iter)
		{
			st.hu_info.ma_pai_all.push_back(*iter);
		}

		for (std::vector<uint16_t>::const_iterator iter = pPlayer->GetHitMa().begin(); iter != pPlayer->GetHitMa().end(); ++iter)
		{
			st.hu_info.ma_pai_hit.push_back(*iter);
		}
		st.event_tile_list.push_back(usParam);
		break;
	}
	case ::msg_maj::ming_gang:
	{
		proRep.add_eventtilelist(usParam);
		proRep.add_eventtilelist(usParam);
		proRep.add_eventtilelist(usParam);
		proRep.add_eventtilelist(usParam);
		BrocastMsg(::comdef::msg_maj, ::msg_maj::response_resp, proRep);

		st.victim_seat = m_pMaj->m_usCurActionPos;
		st.event_tile_list.push_back(usParam);
		st.event_tile_list.push_back(usParam);
		st.event_tile_list.push_back(usParam);
		st.event_tile_list.push_back(usParam);

		m_pMaj->m_nLastGangType = ::msg_maj::ming_gang;
		m_pMaj->m_nLastGangPai = usParam;
		m_pMaj->m_nLastGangPos = pPlayer->GetSeat();
		m_pMaj->m_nLastGangFromPos = m_pMaj->m_usCurActionPos;

		m_pMaj->AddOutPai(usParam, 3);

		pPlayer->m_nLastMingPai = usParam;
		pPlayer->m_nLastMingFromPos = m_pMaj->m_usCurActionPos;
		pPlayer->m_nLastMingTime = time(NULL);

		if (pPlayer->MingGang(usParam))
		{
			pPlayer->MingGangSeated(m_pMaj->GetUsCurActionPos());
			CountEventGang(pPlayer->GetSeat(), ::msg_maj::ming_gang);
			DelDiscardTile();
			m_pMaj->m_bActionEvent = true;
			m_pMaj->SetSendCardPos(pPlayer->GetSeat());
			StartTimerSendCard();
			m_pMaj->m_usGang = 2;
		}
		break;
	}
	case ::msg_maj::an_gang:
	{
		if (pPlayer->AnGang(usParam))
		{
			proRep.add_eventtilelist(usParam);
			proRep.add_eventtilelist(usParam);
			proRep.add_eventtilelist(usParam);
			proRep.add_eventtilelist(usParam);
			BrocastMsg(::comdef::msg_maj, ::msg_maj::response_resp, proRep);

			st.event_tile_list.push_back(usParam);
			st.event_tile_list.push_back(usParam);
			st.event_tile_list.push_back(usParam);
			st.event_tile_list.push_back(usParam);

			CountEventGang(pPlayer->GetSeat(), ::msg_maj::an_gang);

			m_pMaj->m_nLastGangType = ::msg_maj::an_gang;
			m_pMaj->m_nLastGangPai = usParam;
			m_pMaj->m_nLastGangPos = pPlayer->GetSeat();
			m_pMaj->m_nLastGangFromPos = pPlayer->GetSeat();

			m_pMaj->AddOutPai(usParam, 4);

			pPlayer->m_nLastAnPai = usParam;
			pPlayer->m_nLastAnFromPos = m_pMaj->m_usCurActionPos;
			pPlayer->m_nLastAnTime = time(NULL);

			if (!m_pMaj->CheckHasPai())
			{
				BrocastMsg(::comdef::msg_maj, ::msg_maj::response_resp, proRep);
				End();
			}
			else
			{
				m_pMaj->SetSendCardPos(pPlayer->GetSeat());
				StartTimerSendCard();
			}

			m_pMaj->m_usGang = 1;

		}

		break;
	}
	case ::msg_maj::guo_shou_gang:
	{
		if (!pPlayer->CheckNextGang(usParam))
		{
			return;
		}

		proRep.add_eventtilelist(usParam);
		proRep.add_eventtilelist(usParam);
		proRep.add_eventtilelist(usParam);
		proRep.add_eventtilelist(usParam);
		BrocastMsg(::comdef::msg_maj, ::msg_maj::response_resp, proRep);

		st.victim_seat = usParam;
		st.event_tile_list.push_back(usParam);
		st.event_tile_list.push_back(usParam);
		st.event_tile_list.push_back(usParam);
		st.event_tile_list.push_back(usParam);

		m_pMaj->m_nLastGangType = ::msg_maj::guo_shou_gang;
		m_pMaj->m_nLastGangPai = usParam;
		m_pMaj->m_nLastGangPos = pPlayer->GetSeat();
		m_pMaj->m_nLastGangFromPos = pPlayer->GetSeat();

		m_pMaj->AddOutPai(usParam, 1);

		pPlayer->m_nLastGuoShouPai = usParam;
		pPlayer->m_nLastGuoShouFromPos = m_pMaj->m_usCurActionPos;
		pPlayer->m_nLastGuoShouTime = time(NULL);

		//抢杠
		m_pMaj->m_vecSeatEvent.clear();
		bool bHasEvent = false;
		if (m_pRule->QiangGangHuBao3Jia())
		{
			m_pMaj->m_vecSeatEvent.clear();
			m_pMaj->ClearHuPai();

			for (uint16_t i = 0; i < m_usRoomPersons; ++i)
			{
				if (i == pPlayer->GetSeat() || NULL == m_vecPlayer[i])
				{
					continue;
				}

				if (!m_pRule->HuedCanGang(m_vecPlayer[i]))
				{
					continue;
				}

				std::vector<uint16_t> pailist_copy;
				GetCopyPaiList(pailist_copy, m_vecPlayer[i], usParam);

				stHuPai sthupai;
				sthupai.m_eHupaiType = m_pRule->CheckHupaiAll(m_vecPlayer[i], pailist_copy, m_vecPlayer[i]->GetEventPaiList());

				if (sthupai.m_eHupaiType != ::msg_maj::hu_none) //胡牌
				{
					msg_maj::NotifyPlayerEvent proData;
					sthupai.m_eHupaiWay = m_pRule->GetHuWayQiangGangHu();
					sthupai.m_usHupaiPos = i;
					sthupai.m_usPai = usParam;
					m_pMaj->AddSeatEvent(::msg_maj::zi_mo_hu, i);

					::msg_maj::EventInfo* info = proData.add_event_list();
					info->set_event_t(::msg_maj::zi_mo_hu);
					info->add_event_pai(usParam);
					m_vecPlayer[i]->SendMsgToClient(::comdef::msg_maj, ::msg_maj::notify_player_event, proData);

					m_pMaj->AddHuPai(sthupai);

					bHasEvent = true;
				}
			}
		}

		if (pPlayer->NextGang(usParam, bHasEvent))
		{
			m_pMaj->m_usGang = 3;
			m_pMaj->SetSendCardPos(pPlayer->GetSeat());

			if (bHasEvent)
			{
				AddReplayAction(m_nAllGames - m_nGames + 1, st);
				return;
			}

			CountEventGang(pPlayer->GetSeat(), ::msg_maj::guo_shou_gang);

			StartTimerSendCard();
		}

		break;
	}
	case ::msg_maj::pong:
	{
		if (pPlayer->PengPai(usParam))
		{
			proRep.add_eventtilelist(usParam);
			proRep.add_eventtilelist(usParam);
			proRep.add_eventtilelist(usParam);
			BrocastMsg(::comdef::msg_maj, ::msg_maj::response_resp, proRep);

			st.victim_seat = m_pMaj->m_usCurActionPos;
			st.event_tile_list.push_back(usParam);
			st.event_tile_list.push_back(usParam);
			st.event_tile_list.push_back(usParam);

			DelDiscardTile();
			m_pMaj->m_bActionEvent = true;

			m_pMaj->m_usDicardPos = pPlayer->GetSeat();
			m_pMaj->AddOutPai(usParam, 2);

			pPlayer->m_nLastPengPai = usParam;
			pPlayer->m_nLastPengFromPos = m_pMaj->m_usCurActionPos;
			pPlayer->m_nLastPengTime = time(NULL);

			if (m_pRule->IsGuoGang() && m_pMaj->HasThisEventTypeBySeat(pPlayer->GetSeat(), msg_maj::ming_gang))
			{
				pPlayer->AddGuoGangPai(usParam);
			}

			//统计碰牌数
			CPlayer* pPeng = GetPlayer(m_pMaj->m_usCurActionPos);
			if (pPeng)
			{
				pPeng->AddPengSeat(pPlayer->GetSeat());
			}

			TingPaiDiscardPai(pPlayer);

		}

		break;
	}
	default:
		break;
	}

	if (usEventType != ::msg_maj::pass)
	{
		m_pMaj->SetLoopEvents(m_pMaj->GetLoopEvents() + 1);
	}

	m_pMaj->m_vecSeatEvent.clear();
	m_pMaj->ClearHuPai();

	AddReplayAction(m_nAllGames - m_nGames + 1, st);

	if (usEventType == ::msg_maj::zi_mo_hu || usEventType == ::msg_maj::dian_pao_hu)
	{
		::msg_maj::HuPaiListNotify sendHuList;
		sendHuList.set_seat(pPlayer->GetSeat());
		sendHuList.add_tiles(usParam);

		BrocastMsg(::comdef::msg_maj, ::msg_maj::hu_pailist_notify, sendHuList);

		if (!isReadEnd)
		{
			m_pMaj->SetSendCardPos(pPlayer->GetSeat() + 1);
			StartTimerSendCard();
		}
	}
}

void CRoom::AddRoomInfo(uint64_t record_id, uint32_t nRoomID, int16_t room_type, const ::msg_maj::RoomOption& room_option, time_t startTime)
{
	m_unRecordID = record_id;
	m_unZjRoomID = nRoomID;
	m_nRoomType = room_type;
	m_reconrdTime = startTime;
	m_roomZjOption.CopyFrom(room_option);
}

void CRoom::AddPlayerInfo(const std::vector<stPlayerInfo>& infos)
{
	for (std::vector<stPlayerInfo>::const_iterator it = infos.begin(); it < infos.end(); ++it)
	{
		m_mapPlayerInfos.insert(std::make_pair(it->seat, *it));
	}
}

void CRoom::AddInnRecord(int16_t inn_id, int16_t banker_seat, int16_t dice)
{
	std::map<int16_t, stInnRecord>::iterator it = m_mapInnRecords.find(inn_id);
	if ( it == m_mapInnRecords.end())
	{
		stInnRecord record;
		record.inn_id = inn_id;
		record.banker_seat = banker_seat;
		record.dice = dice;
		m_mapInnRecords.insert(std::make_pair(inn_id, record));
	}
}

void CRoom::AddSeatInfo(int16_t inn_id, const std::vector<stSeatInfo>& seatInfos)
{
	std::map<int16_t, stInnRecord>::iterator it = m_mapInnRecords.find(inn_id);
	if (it != m_mapInnRecords.end())
	{
		stInnRecord& record = it->second;
		for (std::vector<stSeatInfo>::const_iterator it2 = seatInfos.begin(); it2 != seatInfos.end(); ++it2)
		{
			record.seat_info.push_back(*it2);
		}
	}
}


void CRoom::AddReplayAction(int16_t inn_id, const stReplayAction& replay)
{
	std::map<int16_t, stInnReplay>::iterator it = m_mapInnReplays.find(inn_id);
	if (it != m_mapInnReplays.end())
	{
		stInnReplay& record = it->second;
		record.replay_list.push_back(replay);
	}
	else
	{
		stInnReplay stInnRep;
		stInnRep.inn_id = inn_id;
		stInnRep.replay_list.push_back(replay);
		m_mapInnReplays.insert(std::make_pair(inn_id, stInnRep));
	}
}

void CRoom::AddGameResult(int16_t inn_id, const std::vector<stGameResultSeat>& seat_result, bool isDissovle)
{
	std::map<int16_t, stInnRecord>::iterator it = m_mapInnRecords.find(inn_id);
	if (it != m_mapInnRecords.end())
	{
		stInnRecord& record = it->second;
		for (std::vector<stGameResultSeat>::const_iterator it2 = seat_result.begin(); it2 != seat_result.end(); ++it2)
		{
			record.seat_result.push_back(*it2);
		}
	}

	if (isDissovle) // 是否解散
	{
		return;
	}

	// 判断是否流局
	bool isLiuJu = true;
	for (int i = 0; i < seat_result.size(); i++)
	{
		if (seat_result[i].hu_info.hu_way != ::msg_maj::hu_way_none)
		{
			isLiuJu = false;
			break;
		}
	}

	// 将胜负信息计算到排行榜中去
	for (int i = 0; i < seat_result.size(); i++)
	{
		const stGameResultSeat& resultInfo = seat_result[i];
		int32_t totalScore = resultInfo.total_score;
		uint64_t totalUid = m_mapPlayerInfos[resultInfo.seat].uid;
		if (totalUid)
		{
			CPlayer* pPlayer = CWorld::Instance()->GetPlayerByUUID(totalUid);
			if (pPlayer)
			{	
				pPlayer->SetTopScoreTotal(pPlayer->GetTopScoreTotal() + totalScore);
				if (totalScore > pPlayer->GetHisMaxScore())
				{
					pPlayer->SetHisMaxScore(totalScore);
				}

				if (totalScore > pPlayer->GetWeekMaxScore())
				{
					pPlayer->SetWeekMaxScore(totalScore);
				}
				
				pPlayer->OnChangeScore();
				
				//
				if (m_pRule->IsThisInnMyWin(pPlayer))
				{
					pPlayer->SetTopWinsTotal(pPlayer->GetTopWinsTotal() + 1);
					pPlayer->SetWinGames(pPlayer->GetWinGames() + 1);
					pPlayer->SetConGames(pPlayer->GetConGames() + 1);
					pPlayer->OnChangeWins();
				}
				else
				{
					if (!isLiuJu)// 不流局则为中断
					{
						pPlayer->SetConGames(0);
					}
				}

				pPlayer->SetTotalGames(pPlayer->GetTotalGames() + 1);
			}
		}
	}
}

bool CRoom::GetRoomInfo(::msg_maj::RoomInfo& roomInfo)
{
	roomInfo.set_room_id(m_unRoomID);
	roomInfo.set_room_type(m_eRoomType);
	roomInfo.mutable_option()->CopyFrom(m_roomZjOption);
	return true;
}

bool CRoom::GetRoleInfoList(::msg_maj::RoleInfoListS& roomInfo)
{
	if (m_mapPlayerInfos.size() != m_usRoomPersons)
	{
		return false;
	}
	std::map<int16_t, stPlayerInfo>::iterator it = m_mapPlayerInfos.begin();
	for (; it != m_mapPlayerInfos.end(); ++it)
	{
		::msg_maj::RoleInfoS* info = roomInfo.add_role_list();
		if (info)
		{
			info->set_seat(it->second.seat);
			info->set_uid(it->second.uid);
			info->set_nick(it->second.nick);
			info->set_actor_addr(it->second.actor_addr);
		}
	}
	return true;
}

bool CRoom::GetInnRecordS(int16_t inn_id, ::msg_maj::InnRecordS& innRecord)
{
	std::map<int16_t, stInnRecord>::iterator it = m_mapInnRecords.find(inn_id);	// 战线记录
	if ( it == m_mapInnRecords.end()) return false;

	const stInnRecord& recordData = it->second;
	if (recordData.seat_info.size() != m_usRoomPersons) return false;

	if (recordData.seat_result.size() != m_usRoomPersons) return false;

	innRecord.set_inn_id(recordData.inn_id);
	innRecord.set_banker_seat(recordData.banker_seat);
	innRecord.set_dice(recordData.dice);

	// 座位初始牌
	for (size_t i = 0; i < recordData.seat_info.size(); ++i)
	{
		::msg_maj::SeatInfo* info = innRecord.add_seat_info();
		if (info)
		{
			const stSeatInfo& data = recordData.seat_info[i];

			info->set_seat(data.seat);

			for (int n = 0; n < data.hand_tiles.size(); ++n)
			{
				info->add_hand_tiles(data.hand_tiles[n]);
			}

			for (int k = 0; k < data.open_tiles.size(); ++k)
			{
				::msg_maj::OpenTile* openTile = info->add_open_tiles();
				if (openTile)
				{
					const stOpenTile& stOpenTile = data.open_tiles[k];
					openTile->set_type(stOpenTile.event_t);
					for (int m = 0; m < stOpenTile.tile_list.size(); ++m)
					{
						openTile->add_tile_list(stOpenTile.tile_list[m]);
					}
				}
			}

			for (int j = 0; j < data.discard_tiles.size(); ++j)
			{
				info->add_discard_tiles(data.discard_tiles[j]);
			}
			info->set_pstatus(data.pstatus);
			info->set_score(data.score);
			info->set_dingque(data.dingque);
		}
	}

	//座位结束数据
	for (int i = 0; i < recordData.seat_result.size(); ++i)
	{
		::msg_maj::GameResultSeat* seatInfo = innRecord.add_seat_result();
		if ( seatInfo )
		{
			const stGameResultSeat& data = recordData.seat_result[i];
			seatInfo->set_seat(data.seat);
			seatInfo->set_total_score(data.total_score);
			for (std::vector<int16_t>::const_iterator it = data.hand_titles.begin(); it != data.hand_titles.end(); ++it)
			{
				seatInfo->add_hand_tiles(*it);
			}

			for (std::vector<stOpenTile>::const_iterator it = data.open_titles.begin(); it != data.open_titles.end(); ++it)
			{
				::msg_maj::OpenTile* openInfo = seatInfo->add_open_tiles();
				if (openInfo)
				{
					const stOpenTile& openData = *it;
					openInfo->set_type(openData.event_t);
					for (std::vector<int16_t>::const_iterator it2 = openData.tile_list.begin(); it2 != openData.tile_list.end(); ++it2)
					{
						openInfo->add_tile_list(*it2);
					}
				}
			}

			seatInfo->set_an_gang(data.an_gang);
			seatInfo->set_ming_gang(data.ming_gang);
			seatInfo->set_guo_shou_gang(data.guoshou_gang);

			if (data.hu_info.hu_way != ::msg_maj::hu_way_none)
			{
				::msg_maj::HuInfo* huInfo = seatInfo->mutable_hu_info();
				if (huInfo)
				{
					m_pRule->SetResultSeatHuInfo(data, huInfo);
					seatInfo->set_hu_tile(data.hu_tile);
				}
			}

			seatInfo->set_dingque(data.dingque);
			seatInfo->set_game_type(data.game_type);
			for (std::vector<stScoreDetail>::const_iterator it2 = data.score_detail.begin(); it2 != data.score_detail.end(); ++it2)
			{
				FillHuTimesToDetail(*it2,seatInfo->add_score_detail());
			}
		}
	}
	return true;
}

void CRoom::GetInnRecordListS(::msg_maj::InnRecordListS* innRecordListS)
{
	for (std::map<int16_t, stInnRecord>::iterator it = m_mapInnRecords.begin(); it != m_mapInnRecords.end(); ++it)
	{
		::msg_maj::InnRecordS innR;
		if (GetInnRecordS(it->first, innR))
		{
			innRecordListS->add_inn_list()->CopyFrom(innR);
		}
		else
			break;
	}
}

bool CRoom::GetInnReplayS(int16_t inn_id, ::msg_maj::InnReplayActionS& innReplay)
{
	// 回放信息
	std::map<int16_t, stInnReplay>::const_iterator it = m_mapInnReplays.find(inn_id);
	if (it == m_mapInnReplays.end()) return false;

	innReplay.set_inn_id(inn_id);

	for (size_t i = 0; i < it->second.replay_list.size(); ++i)
	{
		const stReplayAction& actionData = it->second.replay_list[i];
		::msg_maj::ReplayAction* actionInfo = innReplay.add_replay_list();
		actionInfo->set_event(actionData.event_t);
		actionInfo->set_actor_seat(actionData.actor_seat);
		actionInfo->set_victim_seat(actionData.victim_seat);
		if (actionData.hu_info.hu_way != ::msg_maj::hu_way_none)
		{
			m_pRule->SetReplayActionHuInfo(actionData, actionInfo->mutable_hu_info());
		}

		// 牌的记录
		for (size_t j = 0; j < actionData.event_tile_list.size(); ++j)
		{
			actionInfo->add_event_tile_list(actionData.event_tile_list[j]);
		}

		// 剩余数量
		if (actionData.event_t == ::msg_maj::deal)
		{
			actionInfo->set_desk_tile_count(actionData.desk_tile_count);
		}
	}
	return true;

}

void CRoom::GetInnReplayS(std::map<int16_t, ::msg_maj::InnReplayActionS>& mapInnReplay)
{
	for (std::map<int16_t, stInnRecord>::iterator it = m_mapInnRecords.begin(); it != m_mapInnRecords.end(); ++it)
	{
		::msg_maj::InnReplayActionS innR;
		if (GetInnReplayS(it->first, innR))
			mapInnReplay.insert(std::make_pair(it->first, innR));
		else
			break;
	}
}

bool CRoom::GetInnRecordSeatInfo(int16_t inn_id, std::map<uint16_t, stGameResultSeat>& mapSeat)
{
	std::map<int16_t, stInnRecord>::iterator it = m_mapInnRecords.find(inn_id);	// 战线记录
	if (it != m_mapInnRecords.end())
	{
		const stInnRecord& recordData = it->second;
		if (recordData.seat_info.size() != m_usRoomPersons) return false;

		if (recordData.seat_result.size() != m_usRoomPersons) return false;

		for (size_t i = 0; i < recordData.seat_result.size(); ++i)
		{
			const stGameResultSeat& seatData = recordData.seat_result[i];
			mapSeat.insert(std::make_pair(seatData.seat, seatData));
		}
		return true;
	}
	return false;
}

void CRoom::GetAllInnRecordSeatScore(std::map<uint16_t, int16_t>& mapSeatScore)
{
	for (std::map<int16_t, stInnRecord>::iterator it = m_mapInnRecords.begin(); it != m_mapInnRecords.end(); ++it)
	{
		const stInnRecord& recordData = it->second;
		if (recordData.seat_info.size() != m_usRoomPersons) return ;

		if (recordData.seat_result.size() != m_usRoomPersons) return ;

		for (size_t i = 0; i < recordData.seat_result.size(); ++i)
		{
			const stGameResultSeat& seatData = recordData.seat_result[i];
			mapSeatScore[seatData.seat] += seatData.total_score;
		}
	}
}

void CRoom::RecordInnZhanJiLocal(bool isDissolve)
{
	//游戏记录
	std::vector<stGameResultSeat> seat_result;
	for (uint16_t i = 0; i < m_usRoomPersons; ++i)
	{
		if (NULL == m_vecPlayer[i]) continue;

		stGameResultSeat seatData;
		seatData.seat = i;
		seatData.total_score = m_vecPlayer[i]->GetTotalFan();
		for (std::vector<uint16_t>::const_iterator iter = m_vecPlayer[i]->GetPaiList().begin(); iter != m_vecPlayer[i]->GetPaiList().end(); ++iter)
		{
			seatData.hand_titles.push_back(*iter);
		}
		for (vecEventPai::const_iterator iter = m_vecPlayer[i]->GetEventPaiList().begin(); iter != m_vecPlayer[i]->GetEventPaiList().end(); ++iter)
		{
			const stEventPai& sst = *iter;
			stOpenTile ssst;
			ssst.event_t = ::msg_maj::event_type(sst.usEventType);
			uint16_t usNum = 1;
			switch (ssst.event_t)
			{
			case ::msg_maj::pong:
				usNum = 3;
				break;
			case ::msg_maj::ming_gang:
			case ::msg_maj::an_gang:
			case ::msg_maj::guo_shou_gang:
				usNum = 4;
				break;
			default:
				usNum = 1;
				break;
			}
			for (uint16_t i = 0; i < usNum; ++i)
			{
				ssst.tile_list.push_back(sst.usPai);
			}
			seatData.open_titles.push_back(ssst);
		}
		seatData.an_gang = m_pRule->GetFanAnGang(m_vecPlayer[i]);
		seatData.ming_gang = m_pRule->GetFanMingGang(m_vecPlayer[i]);
		seatData.guoshou_gang = m_pRule->GetFanNextGang(m_vecPlayer[i]);

		stHuPai sthupai;
		m_pMaj->GetHuPai(i, sthupai);
		if (sthupai.m_eHupaiType != ::msg_maj::hu_none)
		{
			seatData.hu_info.hu_way = sthupai.m_eHupaiWay;
			seatData.hu_info.hu_type = sthupai.m_eHupaiType;
			for (std::vector<uint16_t>::const_iterator iter = m_pMaj->GetMaPaiList().begin(); iter != m_pMaj->GetMaPaiList().end(); ++iter)
			{
				seatData.hu_info.ma_pai_all.push_back(*iter);
			}

			for (std::vector<uint16_t>::iterator iter = m_vecPlayer[i]->GetHitMa().begin(); iter != m_vecPlayer[i]->GetHitMa().end(); ++iter)
			{
				seatData.hu_info.ma_pai_hit.push_back(*iter);
			}

			seatData.hu_info.jiang_ghost_num = m_vecPlayer[i]->GetJiangGhostNum();
			seatData.hu_info.jiejiegao_times = m_pMaj->GetLastBankerTimes(i);
			seatData.hu_info.wuGhostJiaBei = m_vecPlayer[i]->GetInnWuHuaJiaBei();
			if (m_pMaj->GetHuWay() == m_pRule->GetHuWayQiangGangHu())
				seatData.hu_tile = m_pMaj->m_nLastGangPai;
			else
				seatData.hu_tile = m_pMaj->m_usCurActionPai;
		}

		seatData.dingque = m_vecPlayer[i]->m_usDingQueType;
		seatData.game_type = m_usGameType;
		for (std::vector<stScoreDetail>::const_iterator it3 = m_vecPlayer[i]->m_vecScoreDetail.begin(); it3 != m_vecPlayer[i]->m_vecScoreDetail.end(); ++it3)
		{
			seatData.score_detail.push_back(*it3);
		}

		seat_result.push_back(seatData);
	}

	AddGameResult(m_nAllGames - m_nGames + 1, seat_result, isDissolve);
}

void CRoom::SaveInnZhangJiGamedb()
{
	fogs::proto::msg::ZhanjiRecordAdd recordProto;
	if (GetInnRecordS(m_nAllGames - m_nGames + 1, *recordProto.mutable_inn_record()))
	{
		if (GetInnReplayS(m_nAllGames - m_nGames + 1, *recordProto.mutable_inn_replay()))
		{
			if (GetRoomInfo(*recordProto.mutable_room_info()) && GetRoleInfoList(*recordProto.mutable_role_info()))
			{
				recordProto.set_record_id(m_unRecordID);
				recordProto.set_room_id(m_unRoomID);
				recordProto.set_start_time(m_reconrdTime);
				GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::ZhanJiRecordAddRequestID, recordProto);

				//保存日志
				AddRecordToLog(recordProto.room_info(), recordProto.role_info(), recordProto.inn_record());
			}
			else
			{
				ASSERT(0);
			}
		}
	}
	else // 取不到完整的数据，需要检查战绩所给的数据是否完整
	{
		//LOG(ERROR) << "CRoom::GetInnRecordS Fail";
	}
}

void CRoom::GetCopyPaiList(std::vector<uint16_t>& pailist_copy, CPlayer* pPlayer, uint16_t usPai)
{
	pailist_copy = pPlayer->GetPaiList();
	bool bFind = false;
	for (std::vector<uint16_t>::iterator iter = pailist_copy.begin(); iter != pailist_copy.end(); ++iter)
	{
		if ((*iter) > usPai)
		{
			pailist_copy.insert(iter, usPai); bFind = true;
			break;
		}
	}
	if (!bFind) pailist_copy.push_back(usPai);
}

void CRoom::AddRecordToLog(const ::msg_maj::RoomInfo& roomInfo, const ::msg_maj::RoleInfoListS& roleInfo, const ::msg_maj::InnRecordS& innRecordS)
{
	::fogs::proto::msg::MatchUserInfo userProto;

	userProto.set_record_id(m_unRecordID);
	userProto.set_record_time(m_reconrdTime);
	userProto.set_room_no(m_unRoomID);
	userProto.set_inn_id(m_nAllGames - m_nGames + 1);
	userProto.set_inn_time(time(NULL));

	const vecHuPai& huPai = m_pMaj->GetVecHuPai();
	if (!huPai.empty())
	{
		userProto.set_huway(huPai[0].m_eHupaiWay);
		userProto.set_hutype(huPai[0].m_eHupaiType);
		userProto.set_huseat(huPai[0].m_usHupaiPos);

		CPlayer* pPlayer = GetPlayer(huPai[0].m_usHupaiPos);
		if (pPlayer)
		{
			const std::vector<uint16_t>& paiList = pPlayer->GetPaiList();
			for (std::vector<uint16_t>::const_iterator it = paiList.begin(); it != paiList.end(); ++it)
			{
				userProto.mutable_hupaitiles()->add_tile_list(*it);
			}
		}
	}
	else
	{
		userProto.set_huway(0);
		userProto.set_hutype(0);
		userProto.set_huseat(100);
	}

	for (int i = 0; i < roleInfo.role_list_size(); ++i)
	{
		const ::msg_maj::RoleInfoS& role = roleInfo.role_list(i);
		if (role.seat() == 0)
			userProto.set_u_1_id(role.uid());
		else if (role.seat() == 1)
			userProto.set_u_2_id(role.uid());
		else if (role.seat() == 2)
			userProto.set_u_3_id(role.uid());
		else
			userProto.set_u_4_id(role.uid());
	}

	for (int i = 0; i < innRecordS.seat_info_size(); ++i)
	{
		const ::msg_maj::SeatInfo& info = innRecordS.seat_info(i);
		if (info.seat() == 0)
			userProto.set_u_1_total(info.score());
		else if (info.seat() == 1)
			userProto.set_u_2_total(info.score());
		else if (info.seat() == 2)
			userProto.set_u_3_total(info.score());
		else
			userProto.set_u_4_total(info.score());
	}

	for (int i = 0; i < innRecordS.seat_result_size(); ++i)
	{
		const ::msg_maj::GameResultSeat& result = innRecordS.seat_result(i);
		
		CPlayer* pPlayer = GetPlayer(result.seat());
		if(pPlayer == NULL) continue;

		if (result.seat() == 0)
		{
			userProto.set_u_1_score(result.total_score());
			userProto.set_u_1_angang(pPlayer->GetAnGangTimes());
			userProto.set_u_1_minggang(pPlayer->GetMingGangTimes());
			userProto.set_u_1_goushouggang(pPlayer->GetNextGangTimes());
		}
		else if (result.seat() == 1)
		{
			userProto.set_u_2_score(result.total_score());
			userProto.set_u_2_angang(pPlayer->GetAnGangTimes());
			userProto.set_u_2_minggang(pPlayer->GetMingGangTimes());
			userProto.set_u_2_goushouggang(pPlayer->GetNextGangTimes());
		}
		else if (result.seat() == 2)
		{
			userProto.set_u_3_score(result.total_score());
			userProto.set_u_3_angang(pPlayer->GetAnGangTimes());
			userProto.set_u_3_minggang(pPlayer->GetMingGangTimes());
			userProto.set_u_3_goushouggang(pPlayer->GetNextGangTimes());
		}
		else
		{
			userProto.set_u_4_score(result.total_score());
			userProto.set_u_4_angang(pPlayer->GetAnGangTimes());
			userProto.set_u_4_minggang(pPlayer->GetMingGangTimes());
			userProto.set_u_4_goushouggang(pPlayer->GetNextGangTimes());
		}
	}	

	GameService::Instance()->SendToDp(::comdef::msg_ss, ::msg_maj::InnRecordAddLogRequestID, userProto);
}

void CRoom::SendNotifyGhostPaiListResult(CPlayer* pPlayer, bool is_disconn)
{
	::msg_maj::NotifyGhostPaiListResult ghostPaiListProto;
	ghostPaiListProto.set_game_type(m_usGameType);
	ghostPaiListProto.set_desk_tile_count(m_pMaj->GetRemainPaiNum());
	ghostPaiListProto.set_fan_pai(m_pMaj->GetFanPai());
	ghostPaiListProto.set_is_disconn(is_disconn);
	
	const std::set<uint16_t>& ghost_list = m_pRule->GetGhostList();
	for (std::set<uint16_t>::const_iterator it = ghost_list.begin(); it != ghost_list.end(); ++it)
	{
		ghostPaiListProto.add_ghost_pai(*it);
	}
		
	if (pPlayer)
	{
		pPlayer->SendMsgToClient(::comdef::msg_maj, ::msg_maj::notify_ghostpai_result, ghostPaiListProto);
	}
	else
	{
		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (NULL == m_vecPlayer[i] || m_vecPlayer[i]->IsDisconnect())
			{
				continue;
			}
			if (pPlayer && pPlayer->GetCharID() == m_vecPlayer[i]->GetCharID())
			{
				continue;
			}
			if (!m_vecPlayer[i]->IsEnterRoomReady())
			{
				continue;
			}
			m_vecPlayer[i]->SendMsgToClient(::comdef::msg_maj, ::msg_maj::notify_ghostpai_result, ghostPaiListProto);
		}
	}
}

void CMaJiang::AddOutPai(uint16_t usPai, uint16_t usNum)
{
	std::map<uint16_t, uint16_t>::iterator it = m_mapOutPai.find(usPai);
	if (it == m_mapOutPai.end())
	{
		m_mapOutPai.insert(std::make_pair(usPai, usNum));
	}
	else
	{
		it->second += usNum;
	}
}

void CMaJiang::CheckLastPai(uint16_t usPai, const std::vector<uint16_t>& vecPaiList, const vecEventPai& eventList, std::pair<uint16_t, uint16_t>& o_lastPaiPair)
{
	uint16_t usOutNum = 0;
	uint16_t usOutDest = 0;
	std::map<uint16_t, uint16_t>::iterator it = m_mapOutPai.find(usPai);
	if (it != m_mapOutPai.end())
	{
		usOutNum = it->second;
		usOutDest = it->second;
	}

	for (std::vector<uint16_t>::const_iterator it = vecPaiList.begin(); it != vecPaiList.end(); ++it)
	{
		if (usPai == *it)
		{
			usOutNum += 1;
		}
	}

	if (usOutNum >= 4)
	{
		usOutNum = 4;
	}

	o_lastPaiPair.first = 4 - usOutDest;
	o_lastPaiPair.second = 4 - usOutNum;
}

void CRoom::GetTingPaiData(CPlayer* pPlayer, std::vector< ::msg_maj::PromptPai>& vecPromptPai, const std::vector<uint16_t>& pailist_copy)
{
	std::set<uint16_t> tinglist;
	int16_t ehutype = m_pRule->CheckTingPai(pailist_copy, tinglist);
	if (ehutype != 0)
	{
		//胡牌基础分数
		for (std::set<uint16_t>::iterator iter = tinglist.begin(); iter != tinglist.end(); ++iter)
		{
			std::vector<uint16_t> pailist_copy_ting = pailist_copy;
			uint16_t usTingPai = *iter;

			bool bFind = false;
			for (std::vector<uint16_t>::iterator iter2 = pailist_copy_ting.begin(); iter2 != pailist_copy_ting.end(); ++iter2)
			{
				if ((*iter2) > usTingPai)
				{
					pailist_copy_ting.insert(iter2, usTingPai);
					bFind = true;
					break;
				}
			}
			if (!bFind)
			{
				pailist_copy_ting.push_back(usTingPai);
			}

			::msg_maj::hu_type huType = m_pRule->CheckHupaiAll(pPlayer, pailist_copy_ting, pPlayer->GetEventPaiList());
			if (huType != ::msg_maj::hu_none)
			{
				// 剩余数量
				std::pair<uint16_t, uint16_t> lastPaiPair;
				m_pMaj->CheckLastPai(usTingPai, pPlayer->GetPaiList(), pPlayer->GetEventPaiList(), lastPaiPair);

				// 算胡牌类型
				int32_t usBigHuMulti = CHuScore::Instance()->GetPaiXingScore(huType);

				::msg_maj::PromptPai pPrompt;
				pPrompt.set_hupai(usTingPai);
				pPrompt.set_paidest(lastPaiPair.first);
				pPrompt.set_painum(lastPaiPair.second);
				pPrompt.set_hutype(huType);
				pPrompt.set_mulit(usBigHuMulti);

				vecPromptPai.push_back(pPrompt);

			}
		}
	}
}

void CRoom::TingPaiDiscardPai(CPlayer* pPlayer)
{
	//听牌
	if (!m_pRule->IsGhost())
	{
		pPlayer->m_tingPaiNotify.Clear();
		pPlayer->m_tingPaiListNotify.Clear();
		const std::vector<uint16_t>& pailist = pPlayer->GetPaiList();
		uint16_t usOutPai = 0;
		for (uint16_t i = 0; i < pailist.size(); ++i)
		{
			if (pailist[i] == usOutPai)
			{
				continue;
			}

			std::vector<uint16_t> pailist_copy = pailist;
			for (std::vector<uint16_t>::iterator iter = pailist_copy.begin(); iter != pailist_copy.end(); ++iter)
			{
				if (*iter == pailist[i])
				{
					pailist_copy.erase(iter);
					break;
				}
			}

			usOutPai = pailist[i];

			std::vector< ::msg_maj::PromptPai> vecPromptPai;
			GetTingPaiData(pPlayer, vecPromptPai, pailist_copy);
			if (vecPromptPai.empty())
			{
				continue;
			}

			::msg_maj::TingPai* tingPai = pPlayer->m_tingPaiListNotify.add_ting_list();
			tingPai->set_dicard(usOutPai);
			for (std::vector< ::msg_maj::PromptPai>::const_iterator it3 = vecPromptPai.begin(); it3 != vecPromptPai.end(); ++it3)
			{
				tingPai->add_prompt_list()->CopyFrom(*it3);
			}
		}

		pPlayer->SendMsgToClient(::comdef::msg_maj, ::msg_maj::ting_pai_discard_notify, pPlayer->m_tingPaiListNotify);
	}
}

void CRoom::TingPaiWaitePai(CPlayer* pPlayer, bool bTurnMe)
{
	if (!m_pRule->IsGhost())
	{
		std::vector<uint16_t> copy_pai_list = pPlayer->GetPaiList();
		if (bTurnMe)
		{
			for (std::vector<uint16_t>::iterator it = copy_pai_list.begin(); it != copy_pai_list.end();)
			{
				if (m_pMaj->m_usCurActionPai == *it)
				{
					it = copy_pai_list.erase(it);
					break;
				}
				else
					++it;
			}
		}

		std::vector< ::msg_maj::PromptPai> vecPromptPai;
		GetTingPaiData(pPlayer, vecPromptPai, copy_pai_list);

		pPlayer->m_tingPaiNotify.Clear();

		for (std::vector< ::msg_maj::PromptPai>::const_iterator it3 = vecPromptPai.begin(); it3 != vecPromptPai.end(); ++it3)
		{
			pPlayer->m_tingPaiNotify.add_prompt_list()->CopyFrom(*it3);
		}

		pPlayer->SendMsgToClient(::comdef::msg_maj, ::msg_maj::ting_pai_waite_notify, pPlayer->m_tingPaiNotify);
	}
}

void CRoom::FillHuTimesToDetail(const stScoreDetail& data, ::msg_maj::ScoreDetail* info)
{
	info->set_type(data.type);
	if (data.type == 1)
	{
		::msg_maj::HuDetail* huInfo = info->mutable_hu();
		const stHuDetail& hu = data.hu;
		huInfo->set_myseat(hu.mySeat);
		huInfo->set_huseat(hu.huSeat);
		huInfo->set_doedseat(hu.doedSeat);
		huInfo->set_score(hu.score);
		huInfo->set_huway(hu.huway);
		huInfo->set_hutype(hu.hutype);
		huInfo->set_humulti(hu.humulti);
		huInfo->set_hutile(hu.huTile);
		huInfo->set_item1(hu.item1);
		huInfo->set_item2(hu.item2);
		huInfo->set_item3(hu.item3);
		huInfo->set_item4(hu.item4);
		huInfo->set_item5(hu.item5);
		huInfo->set_item6(hu.item6);
		huInfo->set_item7(hu.item7);
		huInfo->set_item8(hu.item8);
		huInfo->set_item9(hu.item9);
		huInfo->set_item10(hu.item10);
		huInfo->set_item11(hu.item11);

		for (std::vector<int16_t>::const_iterator it = hu.doedMultiSeat.begin(); it != hu.doedMultiSeat.end(); ++it)
		{
			huInfo->add_doedmultiseat(*it);
		}
	}

	if (data.type == 2)
	{
		::msg_maj::FengYuDetail* fengYuInfo = info->mutable_fengyu();
		const stFengYuDetail& fengyu = data.fengYu;
		fengYuInfo->set_myseat(fengyu.mySeat);
		fengYuInfo->set_huseat(fengyu.huSeat);
		fengYuInfo->set_doedseat(fengyu.doedSeat);
		fengYuInfo->set_score(fengyu.score);
		fengYuInfo->set_fengyutype(fengyu.fengYuType);

		for (std::vector<uint16_t>::const_iterator it = fengyu.doedMultiSeat.begin(); it != fengyu.doedMultiSeat.end(); ++it)
		{
			fengYuInfo->add_doedmultiseat(*it);
		}
	}

	if (data.type == 3)
	{
		::msg_maj::HuaZhuDetail* huazhuInfo = info->mutable_huazhu();
		const stHuaZhuDetail& huazhu = data.huaZhu;
		huazhuInfo->set_myseat(huazhu.mySeat);
		huazhuInfo->set_huseat(huazhu.huSeat);
		huazhuInfo->set_doedseat(huazhu.doedSeat);
		huazhuInfo->set_score(huazhu.score);
	}

	if (data.type == 4)
	{
		::msg_maj::DaJiaoDetail* dajiaoInfo = info->mutable_dajiao();
		const stDaJiaoDetail& dajiao = data.daJiao;
		dajiaoInfo->set_myseat(dajiao.mySeat);
		dajiaoInfo->set_huseat(dajiao.huSeat);
		dajiaoInfo->set_doedseat(dajiao.doedSeat);
		dajiaoInfo->set_score(dajiao.score);
	}

	if (data.type == 5)
	{
		info->set_backtax(data.backTax);
	}

	if (data.type == 6)
	{
		info->set_hujzy(data.huJzy);
	}
}

void CRoom::GetReconnResp(CPlayer* pPlayer, ::msg_maj::ReconnectLoadResp& proMaj)
{
	if (m_eRoomStatus == eRoomStatus_StartGame
		|| m_eRoomStatus == eRoomStatus_SanZhang
		|| m_eRoomStatus == eRoomStatus_DingQue)
	{
		proMaj.set_is_start(true);
		pPlayer->SetSeatInfoProto(proMaj.mutable_self_seat());
		proMaj.set_banker_seat(m_usBankerSeat);
		proMaj.set_dice(m_pMaj->m_unRoll);
		proMaj.set_desk_tile_count(m_pMaj->GetRemainPaiNum());
		proMaj.set_send_disover(pPlayer->IsDisoverCard());
		proMaj.set_self_discard(false);

		//检测自己是否有事件
		bool bHasEvent = false;
		for (vecSeatEvent::iterator iter = m_pMaj->m_vecSeatEvent.begin(); iter != m_pMaj->m_vecSeatEvent.end(); ++iter)
		{
			if ((*iter).usSeat == pPlayer->GetSeat())
			{
				bHasEvent = true;
				if ((*iter).etype == ::msg_maj::zi_mo_hu || (*iter).etype == ::msg_maj::dian_pao_hu)
				{
					if (m_pMaj->m_eActionType == eActionType_SendCard && m_pMaj->m_usCurActionPos == pPlayer->GetSeat())
					{
						proMaj.set_self_discard(true);
						proMaj.set_curr_tile(m_pMaj->m_usCurActionPai);
					}
					::msg_maj::EventInfo* info = proMaj.add_self_events();
					info->set_event_t((*iter).etype);
					info->add_event_pai(m_pMaj->m_usCurActionPai);

				}
				else if ((*iter).etype == ::msg_maj::an_gang)
				{
					proMaj.set_self_discard(true);
					proMaj.set_curr_tile(m_pMaj->m_usCurActionPai);
					std::vector<uint16_t> agpailist;
					m_pRule->CheckAnGang(pPlayer->GetPaiList(), agpailist);
					::msg_maj::EventInfo* info = proMaj.add_self_events();
					info->set_event_t(::msg_maj::an_gang);
					for (std::vector<uint16_t>::iterator it2 = agpailist.begin(); it2 != agpailist.end(); ++it2)
					{
						if (m_pRule->IsGhostCard(*it2))
						{
							if (m_pRule->GhostCanGang())
							{
								info->add_event_pai(*it2);
							}
						}
						else
						{
							info->add_event_pai(*it2);
						}
					}
				}
				else if ((*iter).etype == ::msg_maj::guo_shou_gang)
				{
					std::vector<uint16_t> bgpailist;
					if (m_pRule->CanBuGang(GetPlayer(m_pMaj->m_usCurActionPos), bgpailist))
					{
						::msg_maj::EventInfo* info = proMaj.add_self_events();
						info->set_event_t(::msg_maj::guo_shou_gang);
						for (std::vector<uint16_t>::iterator iter2 = bgpailist.begin(); iter2 != bgpailist.end(); ++iter2)
						{
							info->add_event_pai(*iter2);
						}
					}
					proMaj.set_self_discard(true);
					proMaj.set_curr_tile(m_pMaj->m_usCurActionPai);
				}
				else
				{
					::msg_maj::EventInfo* info = proMaj.add_self_events();
					info->set_event_t((*iter).etype);
					info->add_event_pai(m_pMaj->m_usCurActionPai);
				}
			}
		}

		//没有事件看谁出牌
		if (!bHasEvent && !pPlayer->CheckPaiNum() && m_pMaj->m_usDicardPos == pPlayer->GetSeat())
		{
			proMaj.set_self_discard(true);
			if (m_pMaj->m_bActionEvent)
			{
				uint16_t usSize = pPlayer->GetPaiList().size();
				if (usSize > 0)
				{
					proMaj.set_curr_tile(pPlayer->GetPaiList()[usSize - 1]);
				}
			}
			else
			{
				if (m_pMaj->m_usCurActionPai <= 0)
				{
					proMaj.set_self_discard(false);
				}
				else
				{
					proMaj.set_curr_tile(m_pMaj->m_usCurActionPai);
				}
			}
		}
	}
	else
	{
		proMaj.set_is_start(false);

		for (uint16_t i = 0; i < m_usRoomPersons; ++i)
		{
			if (NULL == m_vecPlayer[i])
			{
				continue;
			}

			::msg_maj::ReconnectPrepare* pPro = proMaj.add_prepare_info();
			pPro->set_seat(i);
			if (m_bStart)
			{
				pPro->set_accept(m_vecPlayer[i]->IsPrepare());
			}
			else
			{
				pPro->set_accept(m_vecPlayer[i]->IsAccept());
			}
			if (m_vecPlayer[i]->IsDisconnect())
			{
				pPro->set_pstatus(::msg_maj::disconnect);
			}
			else
			{
				pPro->set_pstatus(::msg_maj::normal);
			}
		}
	}
}

void CRoom::ReconnectRobot(CPlayer* pPlayer)
{
	//麻将
	::msg_maj::ReconnectLoadResp proMaj;
	GetReconnResp(pPlayer, proMaj);
	pPlayer->SendMsgToClient(::comdef::msg_maj, ::msg_maj::robot_reconnect_resp, proMaj);
}



