#include "CMajingRuleXZ.hpp"

#include "CRoom.hpp"
#include "CPlayer.hpp"
#include "CMaJiang.hpp"
#include "CHuScore.h"


CMajingRuleXZ::CMajingRuleXZ(CRoom* pRoom, const ::msg_maj::RoomOption& roomOption)
	:CMajingRule(pRoom,roomOption)
{
}

CMajingRuleXZ::~CMajingRuleXZ()
{

}

void CMajingRuleXZ::SetRoomRoomOption(const ::msg_maj::RoomOption& m_roomOption)
{
	const ::msg_maj::XZOption& option = m_roomOption.xz_option();


	m_bGhost = false;

	m_bSanZhang = HAS_FLAG(option.mode(), 1);
	m_bHuJiaoZhuanYu = HAS_FLAG(option.mode(), 2);

	m_bIsDaiYj = HAS_FLAG(option.wanfatype(), 1);
	m_bIsDuanYj = HAS_FLAG(option.wanfatype(), 2);
	m_bIsJiangJGG = HAS_FLAG(option.wanfatype(), 4);
	m_bIsMenQing = HAS_FLAG(option.wanfatype(), 8);
	m_bTianDihu = HAS_FLAG(option.wanfatype(), 16);

	m_bDingQue = true;

}

uint16_t CMajingRuleXZ::GetTotalPaiNum(CRoom* pRoom)
{
	uint16_t totalNum = 136;
	switch (m_pRoom->GetTotalPersons())
	{
	case 2:
	case 3:
	case 4:
	{
		totalNum = 108;
		break;
	}
	default:
		break;
	}
	return totalNum;
}

bool CMajingRuleXZ::IsGhostCard(CRoom* pRoom, uint16_t usPai)
{
	return false;
}

uint16_t CMajingRuleXZ::GetBankerSeat()
{
	const vecHuPai& vecHuPai = m_pMaj->GetVecHuPai();
	switch (vecHuPai.size())
	{
	case 0:
	{
		if (m_pRoom->GetTotalPersons() > 0)
		{
			return (m_pRoom->GetBankerSeat() + 1) % m_pRoom->GetTotalPersons();
		}

		break;
	}
	case 1: return vecHuPai[0].m_usHupaiPos; break;
	default: return m_pMaj->GetUsCurActionPos(); break;
	}
	return m_pRoom->GetBankerSeat();
}

// 检测平胡
bool CMajingRuleXZ::CheckHupai_PingHu(const std::vector<uint16_t>& pailist)
{
	return CMajingRule::CheckHupai_PingHu(pailist);
}

// 检测胡牌
::msg_maj::hu_type CMajingRuleXZ::CheckHupaiAll(CPlayer* pPlayer, const std::vector<uint16_t>& pailist, const vecEventPai& eventpailist, uint16_t usTingPai)
{
	return CheckHupaiAndGhost(pPlayer,pailist,eventpailist, usTingPai);
}

// 检测胡牌
::msg_maj::hu_type CMajingRuleXZ::CheckHupaiAndGhost(CPlayer* pPlayer, const std::vector<uint16_t>& pailist, const vecEventPai& eventpailist, uint16_t usTingPai)
{
	Display(pailist);

	if (pailist.size() % 3 != 2)
	{
		//LOG(ERROR) << "CMajingRule::CheckHupai() size: " << pailist.size();
		return ::msg_maj::hu_none;
	}

	// 是否有花猪
	for (std::vector<uint16_t>::const_iterator it = pailist.begin(); it != pailist.end(); ++it)
	{
		if (IsFliterPaiEvent(pPlayer, *it))
		{
			return ::msg_maj::hu_none;
		}
	}

	std::multimap<int16_t, ::msg_maj::hu_type, std::greater<int16_t> > mapScoreType;

	uint16_t score = 0;
	::msg_maj::hu_type huType = ::msg_maj::hu_none;
	bool bRet = m_bGhost ? CheckHupai_QiDui_Ghost(pailist) : CheckHupai_QiDui(pailist);
	if (bRet)
	{
		huType = ::msg_maj::hu_t_xz_qidui;
		score = CHuScore::Instance()->GetPaiXingScore(huType);
		mapScoreType.insert(std::make_pair(score, huType));
		if (IsQys(pailist, eventpailist, m_bGhost))
		{
			huType = ::msg_maj::hu_t_xz_qingqd;
			score = CHuScore::Instance()->GetPaiXingScore(huType);
			mapScoreType.insert(std::make_pair(score, huType));
		}

		if (IsHh(pailist, eventpailist, m_bGhost))
		{
			huType = ::msg_maj::hu_t_xz_longqd;
			score = CHuScore::Instance()->GetPaiXingScore(huType);
			mapScoreType.insert(std::make_pair(score, huType));
		}

		if (IsQys(pailist, eventpailist, m_bGhost) && IsHh(pailist, eventpailist, m_bGhost))
		{
			huType = ::msg_maj::hu_t_xz_qinglqd;
			score = CHuScore::Instance()->GetPaiXingScore(huType);
			mapScoreType.insert(std::make_pair(score, huType));
		}
	}

	if (huType == ::msg_maj::hu_none)
	{
		bool bCanPingHu = m_bGhost ? CheckHupai_PingHu_Ghost(pailist) : CheckHupai_PingHu(pailist);
		if (bCanPingHu)
		{
			huType = ::msg_maj::hu_t_xz_pinghu;
			score = CHuScore::Instance()->GetPaiXingScore(huType);
			mapScoreType.insert(std::make_pair(score, huType));
			if (IsPPHu(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_xz_pphu;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
			}

			if (this->IsDuanYj() && CMajingRule::IsDuanYj(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_xz_duanyj;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
			}

			if (this->IsMengQing() && IsMq(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_xz_mq;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
			}

			if (IsQys(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_xz_qys;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
			}

			if (this->IsDaiYj() && CMajingRule::IsDaiYj(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_xz_daiyj;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
			}

			if (IsJingGG(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_xz_jgg;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
				if (this->IsJiangjgg())
				{
					bool isAll258Pai = true;
					for (vecEventPai::const_iterator it3 = eventpailist.begin(); it3 != eventpailist.end(); ++it3)
					{
						if (!Is258(it3->usPai))
						{
							isAll258Pai = false;
							break;
						}
					}

					if (isAll258Pai)
					{
						bool isJiang258 = m_bGhost ? CheckHupai_PingHu_Ghost_Jiang258(pailist) : CheckHupai_PingHu_Jiang258(pailist);
						if (isJiang258)
						{
							huType = ::msg_maj::hu_t_xz_jjgg;
							score = CHuScore::Instance()->GetPaiXingScore(huType);
							mapScoreType.insert(std::make_pair(score, huType));
						}
					}
				}

				if (IsQys(pailist, eventpailist, m_bGhost))
				{
					huType = ::msg_maj::hu_t_xz_qjgg;
					score = CHuScore::Instance()->GetPaiXingScore(huType);
					mapScoreType.insert(std::make_pair(score, huType));
				}
			}

			if (IsPPHu(pailist, eventpailist, m_bGhost) && IsQys(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_xz_qingdui;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
			}

			if (IsQyj(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_xz_qingyj;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
			}

			if (Is18Lh(pailist, eventpailist, m_bGhost))
			{
				huType = ::msg_maj::hu_t_xz_18lh;
				score = CHuScore::Instance()->GetPaiXingScore(huType);
				mapScoreType.insert(std::make_pair(score, huType));
			}
		}
	}

	if (huType != ::msg_maj::hu_none)
	{
		if (IsDiHu(pPlayer))
		{
			huType = ::msg_maj::hu_t_xz_dihu;
			score = CHuScore::Instance()->GetPaiXingScore(huType);
			mapScoreType.insert(std::make_pair(score, huType));
		}

		if (IsTianHu(pPlayer))
		{
			huType = ::msg_maj::hu_t_xz_tianhu;
			score = CHuScore::Instance()->GetPaiXingScore(huType);
			mapScoreType.insert(std::make_pair(score, huType));
		}

		if (mapScoreType.empty())
		{
			return ::msg_maj::hu_none;
		}
		else
		{
			return mapScoreType.begin()->second;
		}
	}
	return huType;
}

uint16_t CMajingRuleXZ::GetMaPaiNum(uint16_t nMaType)
{
	uint16_t usNum = 0;
	switch (nMaType)
	{
	case 1: usNum = 2; break;
	case 2: usNum = 4; break;
	case 3: usNum = 6; break;
	case 4: usNum = 8; break;
	default:
		break;
	}
	return usNum;
}

eRoomStatus CMajingRuleXZ::AcceptAskAllNextStatus() const
{
	return IsSanZhang() ? eRoomStatus_SanZhang : eRoomStatus_DingQue;
}

::msg_maj::hu_way CMajingRuleXZ::GetHuWayZiMo() const
{
	return m_pMaj->GetRemainPaiNum() < 1 ? ::msg_maj::hu_way_xz_zimo_haidi : ::msg_maj::hu_way_xz_zimo;
}

::msg_maj::hu_way CMajingRuleXZ::GetHuWayDiaoPao() const
{
	return m_pMaj->GetRemainPaiNum() < 1 ? ::msg_maj::hu_way_xz_dianpao_haidi : ::msg_maj::hu_way_xz_dianpao;
}

// 杠分在最后的胡牌才能结算 
void CMajingRuleXZ::CountEventMingGang(uint16_t usSeat)
{
	int32_t nTotalScore = GetBaseScore() * 2;
	
	m_pRoom->GetPlayer(usSeat)->AddTotalFan(nTotalScore);
	m_pRoom->GetPlayer(usSeat)->AddMingGangTimes(1);

	uint16_t curActPos = m_pMaj->GetUsCurActionPos();

	stFengYuDetail stfengyu;
	stfengyu.doedMultiSeat.push_back(getOffsetPos(usSeat, curActPos));
	stfengyu.SetData(usSeat, usSeat, getOffsetPos(usSeat, curActPos), nTotalScore, 1);
	m_pRoom->GetPlayer(usSeat)->AddScore(stfengyu);

	m_pRoom->GetPlayer(curActPos)->AddTotalFan(-nTotalScore);
	m_pRoom->GetPlayer(curActPos)->AddFangGangTimes(1);

	stFengYuDetail stfengyued;
	stfengyued.doedMultiSeat.push_back(getOffsetPos(curActPos, usSeat));
	stfengyued.SetData(curActPos, usSeat, getOffsetPos(curActPos,usSeat), -nTotalScore, 1);
	m_pRoom->GetPlayer(curActPos)->AddScore(stfengyued);

}

void CMajingRuleXZ::CountEventAnGang(uint16_t usSeat)
{
	int32_t nScore = GetBaseScore() * 2;
	int32_t winScore = 0;
	stFengYuDetail stfengyu;
	for (uint16_t i = 0; i < m_pRoom->GetTotalPersons(); ++i)
	{
		if (NULL == m_pRoom->GetPlayer(i) || m_pRoom->GetPlayer(i)->IsHued())
		{
			continue;
		}
		if (i == usSeat)
		{
			continue;
		}

		winScore += nScore;
		stfengyu.doedMultiSeat.push_back(getOffsetPos(usSeat, i));

		m_pRoom->GetPlayer(i)->AddTotalFan(-nScore);
		stFengYuDetail stfengyued;
		stfengyued.SetData(i, usSeat, getOffsetPos(i, usSeat), -nScore, 3);
		m_pRoom->GetPlayer(i)->AddScore(stfengyued);
	}

	m_pRoom->GetPlayer(usSeat)->AddTotalFan(winScore);
	m_pRoom->GetPlayer(usSeat)->AddAnGangTimes(1);

	stfengyu.SetData(usSeat, usSeat, -1, winScore, 3);
	m_pRoom->GetPlayer(usSeat)->AddScore(stfengyu);
}

void CMajingRuleXZ::CountEventGouShouGang(uint16_t usSeat)
{
	int32_t nScore = GetBaseScore() * 1;
	int32_t winScore = 0;
	stFengYuDetail stfengyu;
	for (uint16_t i = 0; i < m_pRoom->GetTotalPersons(); ++i)
	{
		if (NULL == m_pRoom->GetPlayer(i) || m_pRoom->GetPlayer(i)->IsHued())
		{
			continue;
		}
		
		if (i == usSeat)
		{
			continue;
		}

		winScore += nScore;
		stfengyu.doedMultiSeat.push_back(getOffsetPos(usSeat, i));

		m_pRoom->GetPlayer(i)->AddTotalFan(-nScore);
		stFengYuDetail stfengyued;
		stfengyued.SetData(i, usSeat, getOffsetPos(i, usSeat), -nScore, 2);
		m_pRoom->GetPlayer(i)->AddScore(stfengyued);
	}

	m_pRoom->GetPlayer(usSeat)->AddTotalFan(winScore);
	m_pRoom->GetPlayer(usSeat)->AddNextGangTimes(1);

	stfengyu.SetData(usSeat, usSeat, -1, winScore, 2);
	m_pRoom->GetPlayer(usSeat)->AddScore(stfengyu);
}

bool CMajingRuleXZ::CanDianPao(CPlayer* pPlayer)
{ 
	return true;
}

void CMajingRuleXZ::CountResult()
{
	// 总分 = 牌形分 * 胡牌方式
	CMaJiang* pMaj = m_pMaj;
	vecHuPai huPaies = pMaj->GetVecHuPai();
	std::pair<uint16_t, uint16_t> pairhitMaInfo;
	for (vecHuPai::iterator it = huPaies.begin(); it != huPaies.end(); ++it)
	{
		stHuPai& sthupai = *it;
		CPlayer* pHuPlayer = m_pRoom->GetPlayer(sthupai.m_usHupaiPos);
		if (pHuPlayer == NULL)
		{
			continue;
		}
		switch (sthupai.m_eHupaiWay)
		{
		case ::msg_maj::hu_way_xz_zimo:
		case ::msg_maj::hu_way_xz_gangkaihua:
		case ::msg_maj::hu_way_xz_dianpao:
		case ::msg_maj::hu_way_xz_zimo_haidi:
		case ::msg_maj::hu_way_xz_dianpao_haidi:
		{
			// 结算胡牌人的分
			CountHupaiScore(sthupai, pHuPlayer->GetPaiList(), pHuPlayer->GetEventPaiList(), pairhitMaInfo);
			pHuPlayer->AddZiMoTimes(1, pHuPlayer->IsGhostHu(this));
			pHuPlayer->AddHitMaTotal(pairhitMaInfo.first);
			pHuPlayer->AddHuPaiTotal(1);
			break;
		}
		case ::msg_maj::hu_way_xz_gangshangpao:
		case ::msg_maj::hu_way_xz_qiangganghu:
		{
			// 结算胡牌人的分
			CountHupaiScore(sthupai, pHuPlayer->GetPaiList(), pHuPlayer->GetEventPaiList(), pairhitMaInfo);
			pHuPlayer->AddHitMaTotal(pairhitMaInfo.first);
			pHuPlayer->AddHuPaiTotal(1);
			break;
		}
		default:
			break;
		}
	}
}

bool CMajingRuleXZ::CanBuGang(CPlayer* pPlayer, std::vector<uint16_t>& agpailist)
{
	if (NULL == pPlayer)
	{
		//LOG(ERROR) << "CanBuGang() NULL == pPlayer";
		return false;
	}

	const std::vector<uint16_t>& paiList = pPlayer->GetPaiList();
	for (std::vector<uint16_t>::const_iterator it = paiList.begin(); it != paiList.end(); ++it)
	{
		if (pPlayer->CheckNextGang(*it))
		{
			agpailist.push_back(*it);
		}
	}
	return !agpailist.empty();
}

void CMajingRuleXZ::GetHitMaDatas(const stHuPai& sthupai, uint16_t& hitMaTimes)
{
	hitMaTimes = 0;
}

int32_t CMajingRuleXZ::CountHupaiBaseScore(const stHuPai& sthupai, const std::vector<uint16_t>& pailist, const vecEventPai& eventpailist, stHuDetail& huDetail)
{
	//牌形基本番
	int32_t nBaseMulti = CHuScore::Instance()->GetPaiXingScore(sthupai.m_eHupaiType);

	// 是否带根
	uint16_t usGenCount = 0;
	uint16_t usGhostNum = 0;
	vecCheckPai stpailist;
	if (m_bGhost)
		ChangeStructPai_Ghost(pailist, stpailist, usGhostNum);
	else
		ChangeStructPai(pailist, stpailist);
	uint16_t usNum1 = 0, usNum2 = 0, usNum3 = 0, usNum4 = 0;
	GetSamePaiNumNum(stpailist, usNum1, usNum2, usNum3, usNum4);
	if (usNum4)
	{
		usGenCount = usNum4;
	}

	for (vecEventPai::const_iterator it = eventpailist.begin(); it != eventpailist.end(); ++it)
	{
		if (it->usEventType != ::msg_maj::pong) continue;

		for (std::vector<uint16_t>::const_iterator it2 = pailist.begin(); it2 != pailist.end(); ++it2)
		{
			if (*it2 == it->usPai)
			{
				usGenCount += 1;
			}
		}
	}

	// 刮风下雨
	uint16_t usFengCount = 0;
	uint16_t usXiaYuCount = 0;
	const vecEventPai& vecEvtPai = eventpailist;
	for (vecEventPai::const_iterator it = vecEvtPai.begin(); it != vecEvtPai.end(); ++it)
	{
		if (it->usEventType == ::msg_maj::ming_gang || it->usEventType == ::msg_maj::guo_shou_gang)
		{
			usFengCount++;
		}
		else if (it->usEventType == ::msg_maj::an_gang)
		{
			usXiaYuCount += 2;
		}
	}

	// 杠上花
	uint16_t usGangShangHua = (sthupai.m_eHupaiWay == ::msg_maj::hu_way_xz_gangkaihua ? 1 : 0 );

	// 杠上炮
	uint16_t usGangShangPao = (sthupai.m_eHupaiWay == ::msg_maj::hu_way_xz_gangshangpao ? 1 : 0);

	// 抢杠胡
	uint16_t usQiangGangHu = (sthupai.m_eHupaiWay == ::msg_maj::hu_way_xz_qiangganghu ? 1 : 0);

	// 是否海底
	uint16_t usDaidi = (sthupai.m_eHupaiWay == ::msg_maj::hu_way_xz_zimo_haidi ? 1 : 0);
	usDaidi += (sthupai.m_eHupaiWay == ::msg_maj::hu_way_xz_dianpao_haidi ? 1 : 0);

	// 总番数，及最大番数限制
	int32_t totalMulti = nBaseMulti + usGenCount + usFengCount + usXiaYuCount + usGangShangHua + usGangShangPao + usQiangGangHu + usDaidi;

	if (m_pRoom->GetRoomOption().xz_option().multi() == 1)
	{
		totalMulti = (totalMulti <= 8 ? totalMulti : 8);
	}
	else if (m_pRoom->GetRoomOption().xz_option().multi() == 2)
	{
		totalMulti = (totalMulti <= 16 ? totalMulti : 16);
	}
	else if (m_pRoom->GetRoomOption().xz_option().multi() == 3)
	{
		totalMulti = (totalMulti <= 32 ? totalMulti : 32);
	}

	// 底分
	int32_t baseScore = 1;

	// 自摸加番
	uint16_t usZimoMulti = 1;
	if (m_pRoom->GetRoomOption().xz_option().zimo() == 1)
	{
		usZimoMulti = 1;
		baseScore = 1;
	}
	else if (m_pRoom->GetRoomOption().xz_option().zimo() == 2)
	{
		usZimoMulti = 1;
		baseScore += 1;
	}
	else
	{
		usZimoMulti += (sthupai.m_eHupaiWay == ::msg_maj::hu_way_xz_zimo ? 1 : 0);
		usZimoMulti += (sthupai.m_eHupaiWay == ::msg_maj::hu_way_xz_gangkaihua ? 1 : 0);
	}

	// 作记录
	huDetail.item1 = nBaseMulti;
	huDetail.item2 = usGenCount;
	huDetail.item3 = usFengCount;
	huDetail.item4 = usXiaYuCount;
	huDetail.item5 = usGangShangHua;
	huDetail.item6 = usGangShangPao;
	huDetail.item7 = usQiangGangHu;
	huDetail.item8 = usDaidi;
	huDetail.item9 = baseScore;
	huDetail.item10 = totalMulti;
	huDetail.item11 = usZimoMulti;

	//底分* 【（基本番 + 各项加番类型 ）<= 封顶 】* [自摸加倍]

	int32_t nTotalScore = baseScore * totalMulti * usZimoMulti;
	return nTotalScore;
}

void CMajingRuleXZ::CountHupaiScore(const stHuPai& sthupai, const std::vector<uint16_t>& pailist, const vecEventPai& eventpailist, std::pair<uint16_t, uint16_t>& out_mainfo)
{
	CPlayer* pPlayer = m_pRoom->GetPlayer(sthupai.m_usHupaiPos);
	if (pPlayer == NULL)
	{
		return;
	}

	stHuDetail stDetail;
	int32_t nBaseScore = CountHupaiBaseScore(sthupai, pailist, eventpailist, stDetail);
	if (nBaseScore == 0)
	{
		//LOG(ERROR) << "CountHupaiScore nBaseScore = 0 ";
	}

	std::vector<int16_t> doedMultiSeat;
	int16_t winScore = 0;
	int16_t doedSeat = -1;
	for (uint16_t i = 0; i < m_pRoom->GetTotalPersons(); ++i)
	{
		if (NULL == m_pRoom->GetPlayer(i) || m_pRoom->GetPlayer(i)->IsHued())  continue;

		if (i == sthupai.m_usHupaiPos) continue;

		int32_t nScoreTmp = nBaseScore;
		winScore += nScoreTmp;

		switch (sthupai.m_eHupaiWay)
		{
		case ::msg_maj::hu_way_xz_zimo:
		{
			m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
			stDetail.SetData(i, sthupai.m_usHupaiPos, getOffsetPos(i, sthupai.m_usHupaiPos), -nScoreTmp, sthupai);
			m_pRoom->GetPlayer(i)->AddScore(stDetail);
			int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
			doedMultiSeat.push_back(offsetSeat);
			doedSeat = -1;
			break;
		}
		case ::msg_maj::hu_way_xz_gangshangpao:
		{
			if (i == m_pMaj->GetUsCurActionPos())
			{
				int16_t lastGetGangScore = 0;
				if (this->IsHuJiaoZhuanYu())// 把之前杠的分拿过来
				{
					CPlayer* pPlayer = m_pRoom->GetPlayer(m_pMaj->GetUsCurActionPos());
					if (pPlayer)
					{
						lastGetGangScore = pPlayer->GetLastGangScore();
					}
				}

				m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
				stDetail.SetData(i, sthupai.m_usHupaiPos, getOffsetPos(i, sthupai.m_usHupaiPos), -nScoreTmp, sthupai);
				m_pRoom->GetPlayer(i)->AddScore(stDetail);

				int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
				doedMultiSeat.push_back(offsetSeat);

				// 呼叫转移
				if (this->IsHuJiaoZhuanYu())// 把之前杠的分拿过来
				{
					m_pRoom->GetPlayer(sthupai.m_usHupaiPos)->AddTotalFan(lastGetGangScore);
					m_pRoom->GetPlayer(sthupai.m_usHupaiPos)->AddScoreHujzy(lastGetGangScore);

					m_pRoom->GetPlayer(m_pMaj->GetUsCurActionPos())->AddTotalFan(-lastGetGangScore);
					m_pRoom->GetPlayer(m_pMaj->GetUsCurActionPos())->AddScoreHujzy(-lastGetGangScore);
				}
				doedSeat = i;
			}
			else
			{
				winScore -= nScoreTmp;
				continue;
			}
			break;
		}
		case ::msg_maj::hu_way_xz_dianpao:
		{
			if (i == m_pMaj->GetUsCurActionPos())
			{
				m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
				stDetail.SetData(i, sthupai.m_usHupaiPos, getOffsetPos(i, sthupai.m_usHupaiPos), -nScoreTmp, sthupai);
				m_pRoom->GetPlayer(i)->AddScore(stDetail);
				int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
				doedMultiSeat.push_back(offsetSeat);
				doedSeat = i;
			}
			else
			{
				winScore -= nScoreTmp;
				continue;
			}
			break;
		}
		case ::msg_maj::hu_way_xz_qiangganghu: //抢杠胡
		{
			if (i == m_pMaj->m_nLastGangPos)
			{
				m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
				stDetail.SetData(i, sthupai.m_usHupaiPos, getOffsetPos(i, sthupai.m_usHupaiPos), -nScoreTmp, sthupai);
				m_pRoom->GetPlayer(i)->AddScore(stDetail);
				int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
				doedMultiSeat.push_back(offsetSeat);
				doedSeat = m_pMaj->m_nLastGangPos;
			}
			else
			{
				winScore -= nScoreTmp;
			}
			break;
		}
		case ::msg_maj::hu_way_xz_gangkaihua: // 暗杠(无三家),其他杠(有可能包三家)
		{
			// 点杠杠当自摸，扣三家
			if (m_pRoom->GetRoomOption().xz_option().diangang() == 1)// 如果是明杠杠上花，则有可能要包三家
			{
				m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
				stDetail.SetData(i, sthupai.m_usHupaiPos, getOffsetPos(i, sthupai.m_usHupaiPos), -nScoreTmp, sthupai);
				m_pRoom->GetPlayer(i)->AddScore(stDetail);
				int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
				doedMultiSeat.push_back(offsetSeat);
			}
			else 
			{
				if (m_pMaj->m_nLastGangType == ::msg_maj::ming_gang) // 点杠杠当点炮，扣一家)
				{
					if (i == m_pMaj->m_nLastGangFromPos)
					{
						m_pRoom->GetPlayer(m_pMaj->m_nLastGangFromPos)->AddTotalFan(-nScoreTmp);
						stDetail.SetData(m_pMaj->m_nLastGangFromPos, sthupai.m_usHupaiPos, getOffsetPos(m_pMaj->m_nLastGangFromPos, sthupai.m_usHupaiPos), -nScoreTmp, sthupai);
						m_pRoom->GetPlayer(m_pMaj->m_nLastGangFromPos)->AddScore(stDetail);
						int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
						doedMultiSeat.push_back(offsetSeat);
						doedSeat = m_pMaj->m_nLastGangFromPos;
					}
					else
					{
						winScore -= nScoreTmp;
					}
				}
				else // 暗杠、补杠扣三家
				{
					m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
					stDetail.SetData(i, sthupai.m_usHupaiPos, getOffsetPos(i, sthupai.m_usHupaiPos), -nScoreTmp, sthupai);
					m_pRoom->GetPlayer(i)->AddScore(stDetail);
					int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
					doedMultiSeat.push_back(offsetSeat);
				}
			}
			break;
		}
		default:
		{
			m_pRoom->GetPlayer(i)->AddTotalFan(-nScoreTmp);
			stDetail.SetData(i, sthupai.m_usHupaiPos, getOffsetPos(i, sthupai.m_usHupaiPos), -nScoreTmp, sthupai);
			m_pRoom->GetPlayer(i)->AddScore(stDetail);
			int16_t offsetSeat = getOffsetPos(sthupai.m_usHupaiPos, i);
			doedMultiSeat.push_back(offsetSeat);
			break;
		}
		}
	}

	stDetail.doedMultiSeat = doedMultiSeat;
	m_pRoom->GetPlayer(sthupai.m_usHupaiPos)->AddTotalFan(winScore);
	stDetail.SetData(sthupai.m_usHupaiPos, sthupai.m_usHupaiPos, getOffsetPos(sthupai.m_usHupaiPos, doedSeat), winScore, sthupai);
	m_pRoom->GetPlayer(sthupai.m_usHupaiPos)->AddScore(stDetail);

	out_mainfo.first = 0;
	out_mainfo.second = winScore;
}

void CMajingRuleXZ::SetHuInfo(const stHuPai& sthupai,uint16_t seat, ::msg_maj::HuInfo* huInfo, bool isFull)
{
	huInfo->set_game_type(::msg_maj::maj_t_xiezhan);
	huInfo->mutable_xz_info()->set_hutype(::msg_maj::hu_type(sthupai.m_eHupaiType));
	huInfo->mutable_xz_info()->set_huway(::msg_maj::hu_way(sthupai.m_eHupaiWay));
	if (!isFull)
	{
		return;
	}

	uint16_t i = seat;
	
	for (std::vector<uint16_t>::const_iterator iter = m_pMaj->GetMaPaiList().begin(); iter != m_pMaj->GetMaPaiList().end(); ++iter)
	{
		huInfo->mutable_xz_info()->add_ma_pai_all(*iter);
	}
	for (std::vector<uint16_t>::iterator iter = m_pRoom->GetPlayer(i)->GetHitMa().begin(); iter != m_pRoom->GetPlayer(i)->GetHitMa().end(); ++iter)
	{
		huInfo->mutable_xz_info()->add_ma_pai_hit(*iter);
	}
}

void CMajingRuleXZ::SetResultSeatHuInfo(const stGameResultSeat& seatData, ::msg_maj::HuInfo* huInfo)
{
	huInfo->set_game_type(::msg_maj::maj_t_xiezhan);
	huInfo->mutable_xz_info()->set_huway(seatData.hu_info.hu_way);
	huInfo->mutable_xz_info()->set_hutype(seatData.hu_info.hu_type);
	for (std::vector<int16_t>::const_iterator it = seatData.hu_info.ma_pai_all.begin(); it != seatData.hu_info.ma_pai_all.end(); ++it)
	{
		huInfo->mutable_xz_info()->add_ma_pai_all(*it);
	}

	for (std::vector<int16_t>::const_iterator it = seatData.hu_info.ma_pai_hit.begin(); it != seatData.hu_info.ma_pai_hit.end(); ++it)
	{
		huInfo->mutable_xz_info()->add_ma_pai_hit(*it);
	}
}

void CMajingRuleXZ::SetReplayActionHuInfo(const stReplayAction& actionData, ::msg_maj::HuInfo* huInfo)
{
	huInfo->set_game_type(::msg_maj::maj_t_xiezhan);
	huInfo->mutable_xz_info()->set_huway(actionData.hu_info.hu_way);
	huInfo->mutable_xz_info()->set_hutype(actionData.hu_info.hu_type);
	for (int j = 0; j < actionData.hu_info.ma_pai_all.size(); ++j)
	{
		huInfo->mutable_xz_info()->add_ma_pai_all(actionData.hu_info.ma_pai_all[j]);
	}
	for (int j = 0; j < actionData.hu_info.ma_pai_hit.size(); ++j)
	{
		huInfo->mutable_xz_info()->add_ma_pai_hit(actionData.hu_info.ma_pai_hit[j]);
	}
}

eRoomStatus CMajingRuleXZ::SendHandCardsAllNextState() const
{
	return IsSanZhang() ? eRoomStatus_SanZhang : eRoomStatus_DingQue;
}

eRoomStatus CMajingRuleXZ::DisoverCardAllCheckAndDoEvent() const
{
	return IsSanZhang() ? eRoomStatus_SanZhang : eRoomStatus_DingQue;
}

// 查花猪
void CMajingRuleXZ::ChaHuaZhu(CRoom* pRoom)
{
	std::vector<CPlayer*> huaZhuList;
	std::vector<CPlayer*> noHuaZhuList;
	for (uint16_t i = 0; i < m_pRoom->GetTotalPersons(); ++i)
	{
		bool isHuaZhu = false;
		const std::vector<uint16_t>& vecPaiList = m_pRoom->GetPlayer(i)->GetPaiList();
		for (std::vector<uint16_t>::const_iterator it = vecPaiList.begin(); it != vecPaiList.end(); ++it)
		{
			if (m_pRoom->GetPlayer(i)->m_usDingQueType == (*it) / 10)
			{
				m_pRoom->GetPlayer(i)->m_bIsHuaZhu = true;
				huaZhuList.push_back(m_pRoom->GetPlayer(i));
				isHuaZhu = true;
				break;
			}
		}
		if (!isHuaZhu)
		{
			noHuaZhuList.push_back(m_pRoom->GetPlayer(i));
		}
	}

	// 进行赔偿不是花猪的人
	bool isDiFenJiaDi = (m_pRoom->GetRoomOption().xz_option().zimo() == 2 ? true : false);
	uint16_t baseScore = isDiFenJiaDi ? 2 : 1;
	uint16_t winScore = baseScore * 16;
	for (uint16_t i = 0; i < huaZhuList.size(); ++i)
	{
		int16_t huaPos = huaZhuList[i]->GetSeat();
		for (uint16_t j = 0; j < noHuaZhuList.size(); ++j)
		{
			int16_t noHuaPos = noHuaZhuList[j]->GetSeat();
			m_pRoom->GetPlayer(noHuaPos)->AddTotalFan(winScore);
			m_pRoom->GetPlayer(huaPos)->AddTotalFan(-winScore);

			stHuaZhuDetail sthuazhu;
			sthuazhu.SetData(noHuaPos, noHuaPos, getOffsetPos(noHuaPos, huaPos), winScore);
			m_pRoom->GetPlayer(noHuaPos)->AddScore(sthuazhu);

			stHuaZhuDetail sthuazhued;
			sthuazhued.SetData(huaPos, noHuaPos, getOffsetPos(huaPos, noHuaPos), -winScore);
			m_pRoom->GetPlayer(huaPos)->AddScore(sthuazhued);
		}
	}
}

// 查大叫
void CMajingRuleXZ::ChaDaJiao(CRoom* pRoom)
{
	// 有两个未听牌的人会形成流局，未下叫的，给叫牌的人给最大番数的分
	// 找到未下叫的人
	std::vector<CPlayer*> vecNotTingAndNotHuaZhu;
	std::vector<CPlayer*> vecNotHuAndTinging;
	std::vector<CPlayer*> vecNotTingTuiShuai;

	int16_t usNotHuCount = 0;
	for (uint16_t i = 0; i < m_pRoom->GetTotalPersons(); ++i)
	{
		CPlayer* pPlayer = m_pRoom->GetPlayer(i);
		if (pPlayer->IsHued())
			continue;

		usNotHuCount++;
		std::vector<uint16_t> pailist_copy;
		m_pRoom->GetCopyPaiList(pailist_copy, m_pRoom->GetPlayer(i), GetGhostCard());
		::msg_maj::hu_type hutype = CheckHupaiAndGhost(pPlayer, pailist_copy, pPlayer->GetEventPaiList(), true);
		if (hutype == ::msg_maj::hu_none) // 未叫牌
		{
			pPlayer->m_bIsNotTing = true;
			if (pPlayer->m_bIsHuaZhu == false)
			{
				vecNotTingAndNotHuaZhu.push_back(pPlayer);
			}
			vecNotTingTuiShuai.push_back(pPlayer);
		}
		else// 已叫牌
		{
			pPlayer->m_usTingMaxFan = CHuScore::Instance()->GetPaiXingScore(hutype);
			vecNotHuAndTinging.push_back(pPlayer);
		}
	}

	// 
	if (usNotHuCount >= 2)
	{
		// 赔偿给未胡在听玩家的最大番
		bool isDiFenJiaDi = (m_pRoom->GetRoomOption().xz_option().zimo() == 2 ? true : false);
		uint16_t baseScore = isDiFenJiaDi ? 2 : 1;

		for (int16_t i = 0; i < vecNotTingAndNotHuaZhu.size(); ++i)
		{
			CPlayer* pPlayerSub = vecNotTingAndNotHuaZhu[i];
			for (int16_t j = 0; j < vecNotHuAndTinging.size(); ++j)
			{
				CPlayer* pPlayerAdd = vecNotHuAndTinging[j];

				uint16_t totalMulti = pPlayerAdd->m_usTingMaxFan;

				if (m_pRoom->GetRoomOption().xz_option().multi() == 1)
				{
					totalMulti = (totalMulti <= 8 ? totalMulti : 8);
				}
				else if (m_pRoom->GetRoomOption().xz_option().multi() == 2)
				{
					totalMulti = (totalMulti <= 16 ? totalMulti : 16);
				}
				else if (m_pRoom->GetRoomOption().xz_option().multi() == 3)
				{
					totalMulti = (totalMulti <= 32 ? totalMulti : 32);
				}

				int16_t winScore = totalMulti;

				pPlayerAdd->AddTotalFan(winScore);
				stDaJiaoDetail stDaJiao;
				stDaJiao.SetData(pPlayerAdd->GetSeat(), pPlayerAdd->GetSeat(), getOffsetPos(pPlayerAdd->GetSeat(), pPlayerSub->GetSeat()), winScore);
				pPlayerAdd->AddScore(stDaJiao);

				pPlayerSub->AddTotalFan(-winScore);
				stDaJiaoDetail stDaJiaoed;
				stDaJiaoed.SetData(pPlayerSub->GetSeat(), pPlayerAdd->GetSeat(), getOffsetPos(pPlayerSub->GetSeat(), pPlayerAdd->GetSeat()), -winScore);
				pPlayerSub->AddScore(stDaJiaoed);
			}
		}

		// 还回查叫的刮风下雨的分
		for (int i = 0; i < vecNotTingTuiShuai.size(); ++i)
		{
			int16_t usGiveBack = 0;
			CPlayer* pPlayer = vecNotTingTuiShuai[i];
			for (std::vector<stScoreDetail>::const_iterator it = pPlayer->m_vecScoreDetail.begin(); it != pPlayer->m_vecScoreDetail.end(); ++it)
			{
				if (it->type == 2)
				{
					const stFengYuDetail& fengYu = it->fengYu;
					if (fengYu.mySeat == pPlayer->GetSeat() && fengYu.huSeat == pPlayer->GetSeat() && fengYu.score > 0)
					{
						for (std::vector<uint16_t>::const_iterator it2 = fengYu.doedMultiSeat.begin(); it2 != fengYu.doedMultiSeat.end(); ++it2)
						{
							uint16_t backSeat = getActualPos(pPlayer->GetSeat(),*it2);
							uint16_t backScore = (fengYu.score / fengYu.doedMultiSeat.size());
							usGiveBack += backScore;
							m_pRoom->GetPlayer(backSeat)->AddTotalFan(backScore);
							m_pRoom->GetPlayer(backSeat)->AddScoreBackTax(backScore);
						}
					}
				}
			}
			if (usGiveBack > 0)
			{
				pPlayer->AddTotalFan(-usGiveBack);
				pPlayer->AddScoreBackTax(-usGiveBack);
			}
		}
	}
}

bool CMajingRuleXZ::IsFliterPaiEvent(CPlayer* pPlayer, uint16_t usPai) const { return pPlayer->m_usDingQueType == usPai / 10; }
bool CMajingRuleXZ::HuedCanSendCard(CPlayer* pPlayer) const { return pPlayer->IsHued() ? false : true; }
bool CMajingRuleXZ::HuedCanDiscard(CPlayer* pPlayer)const { return pPlayer->IsHued() ? false : true; }
bool CMajingRuleXZ::HuedCanEvent(CPlayer* pPlayer) const { return pPlayer->IsHued() ? false : true; }
bool CMajingRuleXZ::HuedCanGang(CPlayer* pPlayer) const { return pPlayer->IsHued() ? false : true; }
bool CMajingRuleXZ::HuedCanPeng(CPlayer* pPlayer) const { return pPlayer->IsHued() ? false : true; }
bool CMajingRuleXZ::HuedCanDiaoPao(CPlayer* pPlayer) const { return pPlayer->IsHued() ? false : true; }
bool CMajingRuleXZ::HuedCanGangShanPao(CPlayer* pPlayer) const { return pPlayer->IsHued() ? false : true; }

bool CMajingRuleXZ::HuedCanEnd(CRoom* pRoom) const 
{ 
	return !m_pMaj->CheckHasPai() || m_pRoom->GetHuPlayerCount() >= 3;
}

bool CMajingRuleXZ::IsThisInnMyWin(CPlayer* pPlayer) const
{
	if (pPlayer->IsHued() && m_pMaj->m_usFirstSeat == pPlayer->GetSeat())
	{
		return true;
	}
	return false;
}









