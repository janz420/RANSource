#include "stdafx.h"
#include "Psapi.h"
#pragma comment( lib, "Psapi.lib" )
#include "./GLAgentServer.h"
#include "./GLItemLMT.h"
#include "./GLGaeaClient.h"
#include "../DbActionLogic.h"

#include "../Lib_ClientUI/Interface/GameTextControl.h"
#include "./GLTaxiStation.h"
#include "./GLClubDeathMatch.h"

/*pvp tyranny, Juver, 2017/08/24 */
#include "./GLPVPTyrannyAgent.h"

/*school wars, Juver, 2018/01/19 */
#include "./GLPVPSchoolWarsAgent.h"

/*pvp capture the flag, Juver, 2018/01/24 */
#include "./GLPVPCaptureTheFlagAgent.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//	Note : 필드 서버 변경시 클라이언트 정보 변경 사항 반영.
//		부활시, gate를 통해 맵 이동시 호출됨.
BOOL GLAgentServer::ReActionMoveFieldSvr ( DWORD dwClientID, DWORD dwGaeaID )
{
	//	Note : 클라이언트의 지속 스킬 리셋.
	//
	GLMSG::SNETPC_SKILLHOLD_RS_BRD NetMsgSkillBrd;
	
	NetMsgSkillBrd.dwID = dwGaeaID;
	NetMsgSkillBrd.emCrow = CROW_PC;
	for ( int i=0; i<SKILLFACT_SIZE; ++i )
	{
		NetMsgSkillBrd.bRESET[i] = true;
	}
	SENDTOCLIENT ( dwClientID, &NetMsgSkillBrd );

	GLMSG::SNETPC_CURESTATEBLOW_BRD NetMsgBlowBrd;
	NetMsgBlowBrd.dwID = dwGaeaID;
	NetMsgBlowBrd.dwID = CROW_PC;
	NetMsgBlowBrd.dwCUREFLAG = DIS_ALL;
	SENDTOCLIENT ( dwClientID, &NetMsgBlowBrd );

	GLMSG::SNETPC_QITEMFACT_END_BRD NetMsgQFactEndBrd;
	SENDTOCLIENT ( dwClientID, &NetMsgQFactEndBrd );

	GLMSG::SNETPC_PKCOMBO_END_BRD NetMsgPKComboBrd;
	SENDTOCLIENT ( dwClientID, &NetMsgPKComboBrd );

	return TRUE;
}

// *****************************************************
// Desc: 필드서버 정보를 체크후 접속 정보 전송(Agent->Field)
// *****************************************************
BOOL GLAgentServer::MsgReqFieldSvrCharChkFb ( NET_MSG_GENERIC* nmg, DWORD _dwClientID, DWORD _dwGaeaID )
{
	GLMSG::SNETPC_FIELDSVR_CHARCHK_FB *pNetMsg = (GLMSG::SNETPC_FIELDSVR_CHARCHK_FB *) nmg;
	PGLCHARAG pChar = GetChar ( pNetMsg->dwGaeaID );
	if ( !pChar )									return FALSE;

	//	Note : 체크된 횟수 LOG.
	//
	++pChar->m_dwFIELDCK_NUM;
	pChar->m_bFIELD_CK[pNetMsg->nChannel][pNetMsg->dwFIELDID] = true;
	if ( pNetMsg->bExist )		++pChar->m_dwFIELDCK_BEING;

	//	Note : 필드 체크 완료 여부 판단. 만족 안하면 대기.
	//
	if ( !pChar->IsFieldCheckComplete(&m_bFIELDSVR[0][0]) )		return TRUE;

	if ( pChar->m_dwFIELDCK_BEING > 0 )
	{
        //	필드서버로 접속이 실패함
		/*
		DEBUGMSG_WRITE( "Found same character in field server. %d removed exist character. ( name %s )",
			pChar->m_dwFIELDCK_BEING, pChar->m_szName );
		*/
	}

	//	Note : 캐릭터 생성위치를 지정.
	//
	SNATIVEID sGenMapID = pChar->m_sStartMapID;
	DWORD dwGenGateID = pChar->m_dwStartGate;
	D3DXVECTOR3 vGenPos = pChar->m_vStartPos;

	//	저장된 위치가 있을 경우 설정함.
	if ( pChar->m_sSaveMapID != NATIVEID_NULL() )
	{
		if ( GLAgentServer::GetInstance().GetFieldServer ( sGenMapID ) != FIELDSERVER_MAX )
		{
			sGenMapID = pChar->m_sSaveMapID;
			dwGenGateID = UINT_MAX;
			vGenPos = pChar->m_vSavePos;

			// MapList 에 Restart flag 가 설정되어 있으면
			// 강제적으로 시작지점으로 이동하게 된다.
			SMAPNODE* pGenMap = m_sMapList.FindMapNode ( sGenMapID );
			if ( !pGenMap )
			{
				DEBUGMSG_WRITE ( "Can't find MapNode in MapsList." );
			}
			else if ( pGenMap->bRestart )
			{
				sGenMapID   = pChar->m_sStartMapID;
				dwGenGateID = pChar->m_dwStartGate;
				vGenPos		= D3DXVECTOR3(0,0,0);
			}
		}

	}

	// 직전에 종료한 맵이 선도전 맵이면 선도클럽이 아닐경우 시작맵으로 지정
	GLAGLandMan* pLAND = GetByMapID ( sGenMapID );
	if ( pLAND )
	{
		if ( pLAND->m_bGuidBattleMap )
		{
			GLGuidance* pGUID = GLGuidanceAgentMan::GetInstance().Find ( pLAND->m_dwClubMapID );
			if ( pGUID && pGUID->m_dwGuidanceClub != pChar->m_dwGuild )
			{
				sGenMapID = pChar->m_sStartMapID;
				dwGenGateID = pChar->m_dwStartGate;
			}
		}

		if ( pLAND->m_bClubDeathMatchMap )
		{
			GLClubDeathMatch* pCDM = GLClubDeathMatchAgentMan::GetInstance().Find ( pLAND->m_dwClubMapID );

			if ( pCDM )
			{
				sGenMapID = pCDM->m_dwClubHallMap;
				dwGenGateID = pCDM->m_dwClubHallGate;
			}
		}

		/*pvp tyranny, Juver, 2017/08/24 */
		if ( pLAND->m_bPVPTyrannyMap )
		{
			sGenMapID = GLPVPTyrannyAgent::GetInstance().m_sOutMap;
			dwGenGateID = GLPVPTyrannyAgent::GetInstance().m_wOutGate;
		}

		/*school wars, Juver, 2018/01/19 */
		if ( pLAND->m_bPVPSchoolWarsMap )
		{
			sGenMapID = GLPVPSchoolWarsAgent::GetInstance().m_sOutMap;
			dwGenGateID = GLPVPSchoolWarsAgent::GetInstance().m_wOutGate;
		}

		/*pvp capture the flag, Juver, 2018/01/31 */
		if ( pLAND->m_bPVPCaptureTheFlagMap )
		{
			sGenMapID = GLPVPCaptureTheFlagAgent::GetInstance().m_sOutMap;
			dwGenGateID = GLPVPCaptureTheFlagAgent::GetInstance().m_wOutGate;
		}
	}

	//	Note : 필드서버 정보를 가져와서 필드서버로 접속
	//
	bool bCANNOTFINDMAP(false);
	DWORD dwFieldServer = GLAgentServer::GetInstance().GetFieldServer ( sGenMapID );
	if ( dwFieldServer == FIELDSERVER_MAX )
	{
		bCANNOTFINDMAP = true;

        //	캐릭터가 생성될 필드서버를 찾지 못함. 그러나 진입할 수 있도록 초기화함 ( 캐릭터별 학교의 0번 게이트 )
		DEBUGMSG_WRITE ( "Can't find field server associate with character (MID[%d/%d], CID[%d] name %s)",
			sGenMapID.wMainID, sGenMapID.wSubID, pChar->m_dwClientID, pChar->m_szName );

		// DBAction 작업 끝나면 바로 제거하고 테스트 해라
		// return FALSE;

		sGenMapID = GLCONST_CHAR::nidSTARTMAP[pChar->m_wSchool];
		dwGenGateID = GLCONST_CHAR::dwSTARTGATE[pChar->m_wSchool];

		GLAGLandMan* pLandMan = GetByMapID ( sGenMapID );
		if ( !pLandMan )
		{
			DEBUGMSG_WRITE ( "Can't find StartMap ID." );
			return FALSE;
		}

		DxLandGateMan* pGateMan = pLandMan->GetLandGateMan ();
		if ( !pGateMan )
		{
			DEBUGMSG_WRITE ( "Can't Find LandGateManager." );
			return FALSE;

		}
		PDXLANDGATE pGate = pGateMan->FindLandGate ( dwGenGateID );
		if ( !pGate )
		{
			DEBUGMSG_WRITE ( "Can't Find LandGate." );
			vGenPos = D3DXVECTOR3(0,0,0);
		}
		else
		{
			vGenPos = pGate->GetGenPos ( DxLandGate::GEN_RENDUM );
		}

		dwFieldServer = GLAgentServer::GetInstance().GetFieldServer ( sGenMapID );
		if ( dwFieldServer == FIELDSERVER_MAX )
		{
			DEBUGMSG_WRITE ( "Can't Find FieldServer associate with character'school." );
			return FALSE;
		}

		pChar->m_sStartMapID = sGenMapID;
		pChar->m_dwStartGate = dwGenGateID;
		pChar->m_vStartPos   = vGenPos;
		pChar->m_sSaveMapID = sGenMapID;
		pChar->m_vSavePos   = vGenPos;

		// 이부분에서 변경된 스타트지점(MapID, GateID, 위치) 을 DB에 저장함
		CSaveChaPos* pDBAction = new CSaveChaPos ( pChar->m_dwClientID, 
												   pChar->m_dwCharID,
												   dwFieldServer,
												   pChar->m_sStartMapID.dwID,
												   pChar->m_dwStartGate,
												   pChar->m_vStartPos.x,
												   pChar->m_vStartPos.y,
												   pChar->m_vStartPos.z,
												   pChar->m_sSaveMapID.dwID,
												   pChar->m_vSavePos.x,
												   pChar->m_vSavePos.y,
												   pChar->m_vSavePos.z,
												   pChar->m_sSaveMapID.dwID,
												   pChar->m_vSavePos.x,
												   pChar->m_vSavePos.y,
												   pChar->m_vSavePos.z );

		GLDBMan *pDbMan = GLAgentServer::GetInstance().GetDBMan();
		if ( pDbMan )	pDbMan->AddJob ( pDBAction );        
	}

	// 만약 맵을 못 찾았다면 맵을 초기화 하고 다른 곳에서 캐릭터 조인정보를 전송한다
	if ( bCANNOTFINDMAP ) return true;

	if ( m_pMsgServer->ConnectFieldSvr ( pChar->m_dwClientID, dwFieldServer, pChar->m_dwGaeaID, pChar->m_nChannel ) != NET_OK )
	{
        //	캐릭터가 생성될 필드서버로 접속이 실패함
		DEBUGMSG_WRITE ( "Can't connect field. FIELDID : %d, CID[%d] name %s",
			dwFieldServer, pChar->m_dwClientID, pChar->m_szName );
		return FALSE;
	}

	//	캐릭터 조인 정보 전송
	NET_GAME_JOIN_FIELDSVR NetJoinField;
	NetJoinField.emType				= EMJOINTYPE_FIRST;
	NetJoinField.dwSlotAgentClient	= pChar->m_dwClientID;
	StringCchCopy ( NetJoinField.szUID, DAUM_MAX_UID_LENGTH+1, pChar->m_szUserName );
	NetJoinField.nUserNum			= pChar->m_dwUserID;
	NetJoinField.dwUserLvl			= pChar->m_dwUserLvl;
	NetJoinField.nChaNum			= pChar->m_dwCharID;
	NetJoinField.dwGaeaID			= pChar->m_dwGaeaID;
    NetJoinField.tPREMIUM			= pChar->m_tPREMIUM;
	NetJoinField.tCHATBLOCK			= pChar->m_tCHATBLOCK;

	NetJoinField.sStartMap			= sGenMapID;
	NetJoinField.dwStartGate		= dwGenGateID;
	NetJoinField.vStartPos			= vGenPos;

	NetJoinField.dwActState			= pChar->m_dwActState;
	NetJoinField.bUseArmSub			= pChar->m_bUseArmSub;

	NetJoinField.dwThaiCCafeClass	= pChar->m_dwThaiCCafeClass;
	NetJoinField.nMyCCafeClass		= pChar->m_nMyCCafeClass;				// 말레이시아 PC방 이벤트

	NetJoinField.sChinaTime			= pChar->m_sChinaTime;
	NetJoinField.sEventTime			= pChar->m_sEventTime;
	NetJoinField.sVietnamGainSystem = pChar->m_sVietnamSystem;

	//** Add EventTime

	SENDTOFIELD ( pChar->m_dwClientID, &NetJoinField );

	//	Note : 케릭터의 현재 접속 필드서버 및 현재 맵 ID 지정.
	//
	pChar->SetCurrentField ( dwFieldServer, sGenMapID );


	return TRUE;
}

BOOL GLAgentServer::MsgReqGenItemAgt ( NET_MSG_GENERIC* nmg, DWORD dwClientID )
{
	GLMSG::SNET_REQ_GENITEM_AGT *pNetMsg = (GLMSG::SNET_REQ_GENITEM_AGT *) nmg;

	int nCHANNEL = pNetMsg->nCHANNEL;

	const SNATIVEID &sNID = pNetMsg->sNID;
	SITEM* pITEM = GLItemMan::GetInstance().GetItem ( sNID );
	if ( !pITEM )		return FALSE;

	SMAPNODE* pMAPNODE = m_sMapList.FindMapNode ( pNetMsg->sMAPID );
	if ( !pMAPNODE )	return FALSE;

	int nSvrNum = (int) pMAPNODE->dwFieldSID;

	if ( GLITEMLMT::GetInstance().DoCheckItemGen ( sNID ) )
	{
		LONGLONG lnGenCount = GLITEMLMT::GetInstance().RegItemGen ( sNID, pNetMsg->emGENTYPE );

		GLMSG::SNET_REQ_GENITEM_FLD_FB NetMsgFb;
		NetMsgFb.sNID = pNetMsg->sNID;
		NetMsgFb.sMAPID = pNetMsg->sMAPID;
		NetMsgFb.emGENTYPE = pNetMsg->emGENTYPE;
		NetMsgFb.lnGENCOUNT = lnGenCount;
		NetMsgFb.emHoldGroup = pNetMsg->emHoldGroup;
		NetMsgFb.dwHoldGID = pNetMsg->dwHoldGID;
		NetMsgFb.vPos = pNetMsg->vPos;

		SENDTOFIELDSVR ( nCHANNEL, nSvrNum, &NetMsgFb );
	}

	return TRUE;
}

BOOL GLAgentServer::SendToGMMsg( PGLCHARAG pChar, NET_CHAT_FB NetChatFB )
{
	if ( !pChar )	return FALSE;

	UINT i;
	for( i = 0; i < pChar->m_vecGetWhisperList.size(); i++ )
	{
		PGLCHARAG pSendGMChar = GetChar( pChar->m_vecGetWhisperList[i].dwGaeaID );
		if( pSendGMChar == NULL || 
			pSendGMChar->m_GmWhisperList.dwClientID != pChar->m_dwClientID ||
			pSendGMChar->m_GmWhisperList.dwGaeaID != pChar->m_dwGaeaID ||
			pSendGMChar->m_bViewWhisperMSG == FALSE)
		{
			pChar->m_vecGetWhisperList.erase( pChar->m_vecGetWhisperList.begin() + i );
			i--;
			continue;
		}
		SENDTOCLIENT( pChar->m_vecGetWhisperList[i].dwClientID, &NetChatFB );
	}

	return TRUE;
}


BOOL GLAgentServer::ChatMsgProc ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	NET_CHAT* pNetMsg = (NET_CHAT*) nmg;

	switch ( pNetMsg->emType )
	{
	case CHAT_TYPE_GLOBAL: // 관리용 글로벌 메시지
		{
			PGLCHARAG pChar = GetChar ( dwGaeaID );
			if ( !pChar )							return FALSE;
			if ( pChar->m_dwUserLvl < USER_GM3 )	return FALSE;

			NET_CHAT_CTRL_FB NetChatFB;
			NetChatFB.emType = CHAT_TYPE_GLOBAL;
			NetChatFB.sItemLink = pNetMsg->sItemLink; /*item link, Juver, 2017/07/31 */
			StringCchCopy ( NetChatFB.szName, CHR_ID_LENGTH+1, pChar->m_szName ); /*staff name in chat, Juver, 2017/12/26 */
			StringCchCopy ( NetChatFB.szChatMsg, GLOBAL_CHAT_MSG_SIZE+1, pNetMsg->szChatMsg );

			AGCHARNODE *pCharNode = m_PCList.m_pHead;
			for ( ; pCharNode; pCharNode=pCharNode->pNext )
			{
				SENDTOCLIENT ( pCharNode->Data->m_dwClientID, &NetChatFB );
			}
		}
		break;

	case CHAT_TYPE_NORMAL: // 일반메시지
		{
			PGLCHARAG pChar = GetChar ( dwGaeaID );
			if ( !pChar )					return FALSE;
			if ( pChar->IsCHATBLOCK() )
			{
				GLMSG::SNETPC_CHAT_BLOCK NetChatBlock;
				StringCchCopy( NetChatBlock.szName, USR_ID_LENGTH+1, pChar->m_szUserName );
				NetChatBlock.tChatBlock = pChar->m_tCHATBLOCK;

				SENDTOCLIENT ( pChar->m_dwClientID, &NetChatBlock );

				return FALSE;
			}

			//	필드 서버로 전송.
			SENDTOFIELD ( pChar->m_dwClientID, nmg );

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
			if( pChar->m_bTracingUser )
			{
				CDebugSet::ToTracingFile( pChar->m_szUserName, "**Normal Send**, [%s][%s], %s", pChar->m_szUserName, pChar->m_szName, pNetMsg->szChatMsg  );
			}
#endif
		}
		break;

	case CHAT_TYPE_PARTY: // 파티메시지
		{
			PGLCHARAG pChar = GetChar ( dwGaeaID );
			if ( !pChar )							return FALSE;
			if ( pChar->m_dwPartyID==PARTY_NULL )	return FALSE;
			if ( pChar->IsCHATBLOCK() )
			{
				GLMSG::SNETPC_CHAT_BLOCK NetChatBlock;
				StringCchCopy ( NetChatBlock.szName, USR_ID_LENGTH+1, pChar->m_szUserName );
				NetChatBlock.tChatBlock = pChar->m_tCHATBLOCK;

				SENDTOCLIENT ( pChar->m_dwClientID, &NetChatBlock );

				return FALSE;
			}

			NET_CHAT_FB NetChatFB;
			NetChatFB.emType = pNetMsg->emType;
			NetChatFB.sItemLink = pNetMsg->sItemLink; /*item link, Juver, 2017/07/31 */
			StringCchCopy ( NetChatFB.szName, CHR_ID_LENGTH+1, pChar->m_szName );
			StringCchCopy ( NetChatFB.szChatMsg, CHAT_MSG_SIZE+1, pNetMsg->szChatMsg );

			SENDTOPARTY ( pChar->m_dwGaeaID, pChar->m_dwPartyID, (NET_MSG_GENERIC*) &NetChatFB );

			// 자신의 메시지를 보냄
			SendToGMMsg( pChar, NetChatFB );
			//// 상대방의 메시지를 보냄
			//SendToGMMsg( GetChar(pChar->m_dwGaeaID), NetChatFB );

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print

			GLPARTY* pParty = m_cPartyMan.GetParty(pChar->m_dwPartyID);
			if( pParty && pParty->ISVAILD() )
			{
				GLPARTY::MEMBER_ITER iter = pParty->m_cMEMBER.begin();
				GLPARTY::MEMBER_ITER iter_end = pParty->m_cMEMBER.end();

				PGLCHARAG pMemChar = NULL;
				for ( ; iter!=iter_end; ++iter )
				{
					pMemChar = GLAgentServer::GetInstance().GetChar ( (*iter) );
					if ( pMemChar && pMemChar->m_bTracingUser )
					{
						if( pMemChar->m_dwClientID == pChar->m_dwClientID ) continue;

						CDebugSet::ToTracingFile( pMemChar->m_szUserName, "##Party Recv##, [%s][%s]->[%s][%s], %s", 
												  pChar->m_szUserName, pChar->m_szName, pMemChar->m_szUserName, pMemChar->m_szName, pNetMsg->szChatMsg );
					}
				}
			}
			if( pChar->m_bTracingUser )
			{
				CDebugSet::ToTracingFile( pChar->m_szUserName, "##Party Send##, [%s][%s], %s", 
										  pChar->m_szUserName, pChar->m_szName, pNetMsg->szChatMsg  );
			}
#endif
		}
		break;

	case CHAT_TYPE_PRIVATE: // 개인메시지
		{
			PGLCHARAG pChar = GetChar ( dwGaeaID );
			if ( !pChar )							return FALSE;
			if ( pChar->IsCHATBLOCK() )
			{
				GLMSG::SNETPC_CHAT_BLOCK NetChatBlock;
				StringCchCopy ( NetChatBlock.szName, USR_ID_LENGTH+1, pChar->m_szUserName );
				NetChatBlock.tChatBlock = pChar->m_tCHATBLOCK;

				SENDTOCLIENT ( pChar->m_dwClientID, &NetChatBlock );

				return FALSE;
			}

			CHAR_MAP_ITER iterChar = m_PCNameMap.find ( std::string(pNetMsg->szName) );
			if ( iterChar==m_PCNameMap.end() )
			{
				//	귓속말의 대상이 없을때 FB 메시지.
				GLMSG::SNETPC_CHAT_PRIVATE_FAIL NetMsgFail;
				StringCchCopy ( NetMsgFail.szName, CHR_ID_LENGTH+1, pNetMsg->szName );

				SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFail );

				return FALSE;
			}

			//	상대방에게 자기 자신이 귓속말 블럭되어 있는지 검사.
			PGLCHARAG pCHAR_TAR = (*iterChar).second;
			int dwFR_FLAG = pCHAR_TAR->GetStateFriend ( pChar->m_szName );
			
			if ( (dwFR_FLAG==EMFRIEND_BLOCK) && (pChar->m_dwUserLvl>USER_GM3) )
			{
				dwFR_FLAG = EMFRIEND_OFF;
			}

			if ( dwFR_FLAG==EMFRIEND_BLOCK )
			{
				//	해당 사람에게 블럭 되었을때.
				//	귓속말의 대상이 없을때 FB 메시지.
				GLMSG::SNETPC_CHAT_PRIVATE_FAIL NetMsgFail;
				StringCchCopy ( NetMsgFail.szName, CHR_ID_LENGTH+1, pNetMsg->szName );

				SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFail );
			}
			else
			{
				NET_CHAT_FB NetChatFB;
				NetChatFB.emType = pNetMsg->emType;
				NetChatFB.sItemLink = pNetMsg->sItemLink; /*item link, Juver, 2017/07/31 */
				StringCchCopy ( NetChatFB.szName,CHR_ID_LENGTH+1,pChar->m_szName );
				StringCchCopy ( NetChatFB.szChatMsg, CHAT_MSG_SIZE+1, pNetMsg->szChatMsg );

				PGLCHARAG pGLChar = (*iterChar).second;
				SENDTOCLIENT ( pGLChar->m_dwClientID, &NetChatFB );

				// 자신의 메시지를 보냄
				SendToGMMsg( pChar, NetChatFB );
				// 상대방의 메시지를 보냄
				SendToGMMsg( pGLChar, NetChatFB );

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
			
				if( pChar->m_bTracingUser )
				{
					CDebugSet::ToTracingFile( pChar->m_szUserName, "@@Private Send@@, [%s][%s]->[%s][%s], %s", 
						pChar->m_szUserName, pChar->m_szName, pGLChar->m_szUserName, pGLChar->m_szName, pNetMsg->szChatMsg );
				}
				if( pGLChar->m_bTracingUser )
				{
					CDebugSet::ToTracingFile( pGLChar->m_szUserName, "@@Private Recv@@, [%s][%s]<-[%s][%s], %s", 
						pGLChar->m_szUserName, pGLChar->m_szName, pChar->m_szUserName, pChar->m_szName, pNetMsg->szChatMsg );

				}
#endif

			}
		}
		break;

	case CHAT_TYPE_GUILD : // 길드메시지
		{
			PGLCHARAG pChar = GetChar ( dwGaeaID );
			if ( !pChar )							return FALSE;
			if ( pChar->m_dwGuild==CLUB_NULL )		return FALSE;
			if ( pChar->IsCHATBLOCK() )
			{
				GLMSG::SNETPC_CHAT_BLOCK NetChatBlock;
				StringCchCopy ( NetChatBlock.szName, USR_ID_LENGTH+1, pChar->m_szUserName );
				NetChatBlock.tChatBlock = pChar->m_tCHATBLOCK;

				SENDTOCLIENT ( pChar->m_dwClientID, &NetChatBlock );

				return FALSE;
			}

			GLCLUB *pCLUB = m_cClubMan.GetClub ( pChar->m_dwGuild );
			if ( !pCLUB )							return FALSE;

			CLUBMEMBERS_ITER pos = pCLUB->m_mapMembers.begin();
			CLUBMEMBERS_ITER end = pCLUB->m_mapMembers.end();
			for ( ; pos!=end; ++pos )
			{
				PGLCHARAG pMEMBER = GetCharID ( (*pos).first );
				if ( !pMEMBER )		continue;

				NET_CHAT_FB NetChatFB;
				NetChatFB.emType = pNetMsg->emType;
				NetChatFB.sItemLink = pNetMsg->sItemLink; /*item link, Juver, 2017/07/31 */
				StringCchCopy ( NetChatFB.szName,CHR_ID_LENGTH+1,pChar->m_szName );
				StringCchCopy ( NetChatFB.szChatMsg, CHAT_MSG_SIZE+1, pNetMsg->szChatMsg );

				SENDTOCLIENT ( pMEMBER->m_dwClientID, &NetChatFB );

				// 상대방의 메시지를 보냄
				SendToGMMsg( pMEMBER, NetChatFB );

				
#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
				if( pMEMBER->m_bTracingUser )
				{
					CDebugSet::ToTracingFile( pMEMBER->m_szUserName, "%%Guild Recv%%, [%s][%s]->[%s][%s], %s", 
						pChar->m_szUserName, pChar->m_szName, pMEMBER->m_szUserName, pMEMBER->m_szName, pNetMsg->szChatMsg );
				}
#endif
			}

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
			if( pChar->m_bTracingUser )
			{
				CDebugSet::ToTracingFile( pChar->m_szUserName, "%%Guild Send%%, [%s][%s], %s", pChar->m_szUserName, pChar->m_szName, pNetMsg->szChatMsg  );
			}
#endif

			NET_CHAT_FB NetChatFB;
			NetChatFB.emType = pNetMsg->emType;
			NetChatFB.sItemLink = pNetMsg->sItemLink; /*item link, Juver, 2017/07/31 */
			StringCchCopy ( NetChatFB.szName,CHR_ID_LENGTH+1,pChar->m_szName );
			StringCchCopy ( NetChatFB.szChatMsg, CHAT_MSG_SIZE+1, pNetMsg->szChatMsg );

			// 자신의 메시지를 보냄
			// SendToGMMsg( pChar, NetChatFB );

		}
		break;

	case CHAT_TYPE_ALLIANCE:
		{
			PGLCHARAG pChar = GetChar ( dwGaeaID );
			if ( !pChar )							return FALSE;
			if ( pChar->m_dwGuild==CLUB_NULL )		return FALSE;
			if ( pChar->IsCHATBLOCK() )
			{
				GLMSG::SNETPC_CHAT_BLOCK NetChatBlock;
				StringCchCopy ( NetChatBlock.szName, USR_ID_LENGTH+1, pChar->m_szUserName );
				NetChatBlock.tChatBlock = pChar->m_tCHATBLOCK;

				SENDTOCLIENT ( pChar->m_dwClientID, &NetChatBlock );

				return FALSE;
			}

			GLCLUB *pCLUB = m_cClubMan.GetClub ( pChar->m_dwGuild );
			if ( !pCLUB )							return FALSE;
			if ( pCLUB->m_dwAlliance==CLUB_NULL )	return FALSE;

			GLCLUB *pCLUB_CHIEF = m_cClubMan.GetClub ( pCLUB->m_dwAlliance );
			if ( !pCLUB_CHIEF )						return FALSE;

			CLUB_ALLIANCE_ITER pos_alliance = pCLUB_CHIEF->m_setAlliance.begin();
			CLUB_ALLIANCE_ITER end_alliance = pCLUB_CHIEF->m_setAlliance.end();
			for ( ; pos_alliance!=end_alliance; ++pos_alliance )
			{
				const GLCLUBALLIANCE &sALLIANCE = (*pos_alliance);
				GLCLUB *pCLUB_INDIAN = m_cClubMan.GetClub ( sALLIANCE.m_dwID );
				if ( !pCLUB_INDIAN )	continue;

				CLUBMEMBERS_ITER pos = pCLUB_INDIAN->m_mapMembers.begin();
				CLUBMEMBERS_ITER end = pCLUB_INDIAN->m_mapMembers.end();
				for ( ; pos!=end; ++pos )
				{
					PGLCHARAG pMEMBER = GetCharID ( (*pos).first );
					if ( !pMEMBER )		continue;

					NET_CHAT_FB NetChatFB;
					NetChatFB.emType = pNetMsg->emType;
					NetChatFB.sItemLink = pNetMsg->sItemLink; /*item link, Juver, 2017/07/31 */
					StringCchCopy ( NetChatFB.szName,CHR_ID_LENGTH+1,pChar->m_szName );
					StringCchCopy ( NetChatFB.szChatMsg, CHAT_MSG_SIZE+1, pNetMsg->szChatMsg );

					SENDTOCLIENT ( pMEMBER->m_dwClientID, &NetChatFB );

					SendToGMMsg( pMEMBER, NetChatFB );

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
					if( pMEMBER->m_bTracingUser )
					{
						CDebugSet::ToTracingFile( pMEMBER->m_szUserName, "**Alliance Recv**, [%s][%s]->[%s][%s], %s", 
							pChar->m_szUserName, pChar->m_szName, pMEMBER->m_szUserName, pMEMBER->m_szName, pNetMsg->szChatMsg );
					}
#endif
				}
			}

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
			if( pChar->m_bTracingUser )
			{
				CDebugSet::ToTracingFile( pChar->m_szUserName, "**Alliance Send**, [%s][%s], %s", pChar->m_szUserName, pChar->m_szName, pNetMsg->szChatMsg  );
			}
#endif
		}
		break;


		/*regional chat, Juver, 2017/12/06 */
	case CHAT_TYPE_REGIONAL: 
		{
			PGLCHARAG pChar = GetChar ( dwGaeaID );
			if ( !pChar )							return FALSE;
			if ( pChar->IsCHATBLOCK() )
			{
				GLMSG::SNETPC_CHAT_BLOCK NetChatBlock;
				StringCchCopy ( NetChatBlock.szName, USR_ID_LENGTH+1, pChar->m_szUserName );
				NetChatBlock.tChatBlock = pChar->m_tCHATBLOCK;
				SENDTOCLIENT ( pChar->m_dwClientID, &NetChatBlock );
				return FALSE;
			}

			/*pvp capture the flag, Juver, 2018/02/08 */
			BOOL bCaptureTheFlag = FALSE;
			GLAGLandMan* pCurLandMan = GetByMapID ( pChar->m_sCurMapID );
			if ( pCurLandMan )	bCaptureTheFlag = pCurLandMan->m_bPVPCaptureTheFlagMap;

			AGCHARNODE *pCharNode = m_PCList.m_pHead;
			for ( ; pCharNode; pCharNode=pCharNode->pNext )
			{
				if ( !pCharNode->Data )	continue;
				if ( pCharNode->Data->m_sCurMapID.dwID != pChar->m_sCurMapID.dwID ) continue;

				if ( bCaptureTheFlag ){
					if ( pCharNode->Data->m_wCaptureTheFlagTeam != pChar->m_wCaptureTheFlagTeam ) continue;
				}else{
					if ( pCharNode->Data->m_wSchool != pChar->m_wSchool ) continue;
				}
				
				NET_CHAT_FB NetChatFB;
				NetChatFB.emType = pNetMsg->emType;
				NetChatFB.sItemLink = pNetMsg->sItemLink; /*item link, Juver, 2017/07/31 */
				StringCchCopy ( NetChatFB.szName,CHR_ID_LENGTH+1,pChar->m_szName );
				StringCchCopy ( NetChatFB.szChatMsg, CHAT_MSG_SIZE+1, pNetMsg->szChatMsg );
				SENDTOCLIENT ( pCharNode->Data->m_dwClientID, &NetChatFB );

				SendToGMMsg( pCharNode->Data, NetChatFB );

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
				if( pCharNode->Data && pCharNode->Data->m_bTracingUser )
				{
					CDebugSet::ToTracingFile( pCharNode->Data->m_szUserName, "**Regional Recv**, [%s][%s]->[%s][%s], %s", 
						pChar->m_szUserName, pChar->m_szName, pCharNode->Data->m_szUserName, pCharNode->Data->m_szName, pNetMsg->szChatMsg );
				}
#endif
			}

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
			if( pChar->m_bTracingUser )
			{
				CDebugSet::ToTracingFile( pChar->m_szUserName, "**Regional Send**, [%s][%s], %s", pChar->m_szUserName, pChar->m_szName, pNetMsg->szChatMsg  );
			}
#endif

		}break;

	default:
		break;
	}

	return TRUE;
}

BOOL GLAgentServer::MsgReqCreateInstantMap ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{

	

	GLMSG::SNETREQ_CREATE_INSTANT_MAP_REQ *pNetMsg = (GLMSG::SNETREQ_CREATE_INSTANT_MAP_REQ *) nmg;
	GLMSG::SNETREQ_CREATE_INSTANT_MAP_FB NetMsgFB;

	PGLCHARAG pChar = GetChar ( pNetMsg->dwGaeaID );
	if ( !pChar )							return FALSE;

	//	Note : 현재 맵 정보.
	//
	GLAGLandMan* pCurLandMan = GetByMapID ( pChar->m_sCurMapID );
	if ( !pCurLandMan )
	{
		CDebugSet::ToLogFile( "EMCHAR_GATEOUT_MAPID %d, %d", pChar->m_dwCharID, pChar->m_sCurMapID );

		NetMsgFB.emFB = EMCHAR_CREATE_INSTANT_MAP_FAIL_MAPID;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}

	// 현재 게이트
	DxLandGateMan* pCurGateMan = pCurLandMan->GetLandGateMan ();
	PDXLANDGATE pCurGate = pCurGateMan->FindLandGate ( pNetMsg->dwGateID );
	if ( !pCurGate )
	{
		CDebugSet::ToLogFile( "EMCHAR_GATEOUT_GATEID %d %d %d", pChar->m_dwCharID, 
			pChar->m_sCurMapID, pNetMsg->dwGateID );

		NetMsgFB.emFB = EMCHAR_CREATE_INSTANT_MAP_FAIL_GATEID;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Note : 목표 맵.
	//
	GLAGLandMan* pNextLandMan = GetByMapID ( pCurGate->GetToMapID() );
	if ( !pNextLandMan )
	{
		CDebugSet::ToLogFile( "EMCHAR_GATEOUT_TARMAPID %d %d %d %d", 
			pChar->m_dwCharID, pChar->m_sCurMapID, pCurGate->GetGateID(), pCurGate->GetToMapID() );

		NetMsgFB.emFB = EMCHAR_CREATE_INSTANT_MAP_FAIL_TARMAPID;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Note : 목표 게이트.
	//
	DxLandGateMan* pNextGateMan = pNextLandMan->GetLandGateMan ();
	PDXLANDGATE pNextGate = pNextGateMan->FindLandGate ( pCurGate->GetToGateID() );
	if ( !pNextGate )
	{
		CDebugSet::ToLogFile( "EMCHAR_GATEOUT_TARGATEID %d, %d %d %d %d", 
			pChar->m_dwCharID, pChar->m_sCurMapID, pNetMsg->dwGateID, pCurGate->GetToMapID(), 
			pCurGate->GetToGateID() );

		NetMsgFB.emFB = EMCHAR_CREATE_INSTANT_MAP_FAIL_TARMAPID;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}
	SMAPNODE* pMapNode = m_sMapList.FindMapNode (  pCurGate->GetToMapID() );
	if( !pMapNode->bInstantMap )
	{
		NetMsgFB.emFB = EMCHAR_CREATE_INSTANT_MAP_FAIL_NOT;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}
	DWORD dwNextFieldSvr = GetFieldServer ( pCurGate->GetToMapID() );
	if( pChar->m_dwCurFieldSvr != dwNextFieldSvr )
	{
		NetMsgFB.emFB = EMCHAR_CREATE_INSTANT_MAP_FAIL;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}

	/*m_sTestInstantMapCharInfo.dwGaeaID	= pNetMsg->dwGaeaID;
	m_sTestInstantMapCharInfo.sBaseMapID = pCurGate->GetToMapID();

	return TRUE;*/

	GLMSG::SNETREQ_CREATE_INSTANT_MAP_FLD NetMsgFld; 
	SNATIVEID sInstantMapID;

	// 인스턴트 던전을 생성하기전에 정말 생성을 해야하는지 여부를 한번더 판단한다.
	if( pNetMsg->bCreate )
	{
		/*VEC_INSTANT_MAPID_ITER iter = m_vecInstantMapId.begin();
		bool  bEnterInstantMap = FALSE;
		for( ; iter != m_vecInstantMapId.end(); ++iter )
		{
			bEnterInstantMap = FALSE;

			SNATIVEID sNativeID = *iter;
			GLAGLandMan* pLandMan = GetByMapID( sNativeID );
			if( !pLandMan ) continue;

			if( pLandMan->GetMapID().wMainID == pCurGate->GetToMapID().wMainID && !pLandMan->IsInstantMap() ) continue;

			if( pLandMan->IsPartyInstantMap() )
			{
				if( pLandMan->GetInstantMapHostID() == pChar->GetPartyID() )
					bEnterInstantMap = TRUE;
			}else if( pChar->GetPartyID() == PARTY_NULL ){
				if( pLandMan->GetInstantMapHostID() == pChar->m_dwGaeaID )
					bEnterInstantMap = TRUE;
			}		

			if( bEnterInstantMap )
			{
				pNetMsg->bCreate	   = FALSE;
				pNetMsg->sInstantMapID = sNativeID;
				break;
			}
		}*/
	}

	if( pNetMsg->bCreate )
	{
		
		sInstantMapID.wMainID = pCurGate->GetToMapID().wMainID;
		sInstantMapID.wSubID  = m_sInsertInstantMapId.InsertInduMapID();
		if( sInstantMapID.wSubID == SNATIVEID::ID_NULL )
		{
			CDebugSet::ToFileWithTime( "instancemap.txt", "[INFO AGENT]Create Failed Instant Map! ID reached Max Size! IDSize %d Map ID [%d][%d]", 
								m_sInsertInstantMapId.useMapSubIDSet.size(), sInstantMapID.wMainID, sInstantMapID.wSubID );

			NetMsgFB.emFB = EMCHAR_CREATE_INSTANT_MAP_FAIL_CREATEMAX;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}



		if( CreateInstantMap( pCurGate->GetToMapID(), sInstantMapID, pNetMsg->dwGaeaID, pChar->GetPartyID() ) == E_FAIL )
		{
			NetMsgFB.emFB = EMCHAR_CREATE_INSTANT_MAP_FAIL;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}

		GLAGLandMan* pInLandMan = NULL;
		pInLandMan = GetByMapID ( sInstantMapID );
		if ( !pInLandMan )
		{
			NetMsgFB.emFB = EMCHAR_CREATE_INSTANT_MAP_FAIL;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}

		CDebugSet::ToFileWithTime( "instancemap.txt", "[INFO AGENT]Create Instant Map. MapInfo: ID [%d][%d] PartyMap %d HostID %d", 
			sInstantMapID.wMainID, sInstantMapID.wSubID, pInLandMan->IsPartyInstantMap(), pInLandMan->GetInstantMapHostID() );

		NetMsgFld.bCreate	  = TRUE;
		NetMsgFld.dwGaeaID	  = pNetMsg->dwGaeaID;
		NetMsgFld.dwGateID	  = pNetMsg->dwGateID;
		NetMsgFld.sInstantMapID = sInstantMapID;
		NetMsgFld.sBaseMapID  = pCurGate->GetToMapID();

	}else{
		GLAGLandMan* pInLandMan = GetByMapID( pNetMsg->sInstantMapID );
		if ( !pInLandMan )
		{
			NetMsgFB.emFB = EMCHAR_CREATE_INSTANT_MAP_FAIL;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}

		CDebugSet::ToFileWithTime( "instancemap.txt", "[INFO AGENT]Enter Instant Map. MapInfo: ID [%d][%d] PartyMap %d HostID %d", 
			pNetMsg->sInstantMapID.wMainID, pNetMsg->sInstantMapID.wSubID, pInLandMan->IsPartyInstantMap(), pInLandMan->GetInstantMapHostID() );

		sInstantMapID = pNetMsg->sInstantMapID;

		NetMsgFld.bCreate	  = FALSE;
		NetMsgFld.dwGaeaID	  = pNetMsg->dwGaeaID;
		NetMsgFld.dwGateID	  = pNetMsg->dwGateID;
		NetMsgFld.sInstantMapID = sInstantMapID;
		NetMsgFld.sBaseMapID  = pCurGate->GetToMapID();
	}

	
	// Agent 서버에서 갖고 있는 케릭터 맵정보 변경
	pChar->SetCurrentField ( pChar->m_dwCurFieldSvr, sInstantMapID );

	//	Note : 필드 서버 변경 필요 없음. 현재 필드서버로 인던 맵 생성을 요청
	SENDTOFIELDSVR ( pChar->m_nChannel, pChar->m_dwCurFieldSvr, &NetMsgFld );

	

	return TRUE;
}

BOOL GLAgentServer::MsgReqDeleteInstantMap( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNETREQ_CREATE_INSTANT_MAP_DEL *pNetMsg = (GLMSG::SNETREQ_CREATE_INSTANT_MAP_DEL *) nmg;

	GLAGLandMan *pLandMan = GetByMapID ( pNetMsg->sInstantMapID );
	if( pLandMan == NULL ) return FALSE;
	if( !pLandMan->IsInstantMap() ) return FALSE;

	m_sInsertInstantMapId.DeleteInstantMapID( pNetMsg->sInstantMapID.wSubID );


	VEC_INSTANT_MAPID_ITER MapID_iter = std::find( m_vecInstantMapId.begin(), m_vecInstantMapId.end(), pNetMsg->sInstantMapID.dwID );
	if( MapID_iter != m_vecInstantMapId.end()  ) m_vecInstantMapId.erase(MapID_iter);

	SAFE_DELETE ( pLandMan );
	m_pLandMan[pNetMsg->sInstantMapID.wMainID][pNetMsg->sInstantMapID.wSubID] = NULL;

	CDebugSet::ToFileWithTime( "instancemap.txt", "[INFO AGENT]Delete Instant Map!! Instant Map ID [%d][%d]", 
		pNetMsg->sInstantMapID.wMainID,  pNetMsg->sInstantMapID.wSubID );

	BOOL bErase = m_sMapList.EraseMapList( pNetMsg->sInstantMapID );
	if ( !bErase )
		CDebugSet::ToFileWithTime( "instancemap.txt", "[INFO AGENT]InstanceMap erase to mapslist failed[%d~%d]", 
		pNetMsg->sInstantMapID.wMainID, pNetMsg->sInstantMapID.wSubID );
	else
		CDebugSet::ToFileWithTime( "instancemap.txt", "[INFO AGENT]InstanceMap erase to mapslist[%d~%d]", 
		pNetMsg->sInstantMapID.wMainID, pNetMsg->sInstantMapID.wSubID );

	return TRUE;
	
}


// *****************************************************
// Desc: 이전게이트로 이동 요청 (Field->Agent)
// *****************************************************
BOOL GLAgentServer::MsgReqGateOut ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETREQ_GATEOUT *pNetMsg = (GLMSG::SNETREQ_GATEOUT *) nmg;
	//if ( dwGaeaID!=GAEAID_NULL )			return FALSE;

	PGLCHARAG pChar = GetChar ( pNetMsg->dwGaeaID );
	if ( !pChar )							return FALSE;

	//	Note : 현재 맵 정보.
	//
	GLAGLandMan* pCurLandMan = GetByMapID ( pChar->m_sCurMapID );
	if ( !pCurLandMan )
	{
		CDebugSet::ToLogFile( "EMCHAR_GATEOUT_MAPID %d, %d", pChar->m_dwCharID, pChar->m_sCurMapID );

		GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
		NetMsgFB.emFB = EMCHAR_GATEOUT_MAPID;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}
	
	// 현재 게이트
	DxLandGateMan* pCurGateMan = pCurLandMan->GetLandGateMan ();
	PDXLANDGATE pCurGate = pCurGateMan->FindLandGate ( pNetMsg->dwGateID );
	if ( !pCurGate )
	{
		CDebugSet::ToLogFile( "EMCHAR_GATEOUT_GATEID %d %d %d", pChar->m_dwCharID, 
			pChar->m_sCurMapID, pNetMsg->dwGateID );

		GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
		NetMsgFB.emFB = EMCHAR_GATEOUT_GATEID;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Note : 목표 맵.
	//
	GLAGLandMan* pNextLandMan = GetByMapID ( pCurGate->GetToMapID() );
	if ( !pNextLandMan )
	{
		CDebugSet::ToLogFile( "EMCHAR_GATEOUT_TARMAPID %d %d %d %d", 
			pChar->m_dwCharID, pChar->m_sCurMapID, pCurGate->GetGateID(), pCurGate->GetToMapID() );

		GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
		NetMsgFB.emFB = EMCHAR_GATEOUT_TARMAPID;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}

	// 목표맵이 선두클럽 전투존인데 선도전이 끝났고 선도클럽의 길원이 아니면
	if ( pNextLandMan->m_bGuidBattleMap )
	{
		if ( GLCONST_CHAR::bPKLESS ) 
		{
			GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
			NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}

		GLGuidance *pGuid = GLGuidanceAgentMan::GetInstance().Find ( pNextLandMan->m_dwClubMapID );
		if ( pGuid )
		{

			// 선도전 일때 길트배틀 중이거나 종료시간 미달이면 출입금지
			GLCLUB* pClub = m_cClubMan.GetClub ( pChar->m_dwGuild );
			if ( pClub && pGuid->m_bBattle && ( pClub->GetAllBattleNum() > 0 || !pClub->IsGuidBattle()) )
			{
				GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
				NetMsgFB.emFB = EMCHAR_GATEOUT_CLUBBATTLE;
				SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
				return FALSE;
			}

			GLCLUB* pGuidanceClub = m_cClubMan.GetClub ( pGuid->m_dwGuidanceClub );
			if ( !pGuid->m_bBattle && !pGuid->IsGuidClub(pChar->m_dwGuild) )
			{
				if ( pGuidanceClub )
				{
					// 선도클럽의 동맹이 아니면
					if ( !pGuidanceClub->IsAllianceGuild ( pChar->m_dwGuild ) )
					{
						GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
						NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
						SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
						return FALSE;
					}
				}
			}
		}
	}

	// 목표맵이 선두클럽 전투존인데 선도전이 끝났고 선도클럽의 길원이 아니면
	if ( pNextLandMan->m_bClubDeathMatchMap )
	{
		if ( GLCONST_CHAR::bPKLESS || pChar->m_nChannel != 0) 
		{
			GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
			NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}

		GLClubDeathMatch *pCDM = GLClubDeathMatchAgentMan::GetInstance().Find ( pNextLandMan->m_dwClubMapID );
		if ( pCDM )
		{
			GLCLUB* pClub = m_cClubMan.GetClub ( pChar->m_dwGuild );

			if ( !pCDM->IsEnterMap( pChar->m_dwGuild ) || !pClub )
			{
				GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
				NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
				SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
				return FALSE;
			}			
			
			if ( pClub )
			{
				// 클럽배틀 진행중일때는 못들어간다.
                if ( pClub->GetAllBattleNum() > 0 )
				{
					GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
					NetMsgFB.emFB = EMCHAR_GATEOUT_CLUBBATTLE2;
					SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
					return FALSE;
				}

				if ( !pClub->IsMemberFlgCDM(pChar->m_dwCharID) )
				{
					GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
					NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
					SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
					return FALSE;
				}
			}			
		}
	}

	/*pvp tyranny, Juver, 2017/08/24 */
	if ( pNextLandMan->m_bPVPTyrannyMap )
	{
		if ( GLCONST_CHAR::bPKLESS || pChar->m_nChannel != 0) 
		{
			GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
			NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}

		if ( !GLPVPTyrannyAgent::GetInstance().IsBattle() )
		{
			GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
			NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}
	}

	/*school wars, Juver, 2018/01/19 */
	if ( pNextLandMan->m_bPVPSchoolWarsMap )
	{
		if ( GLCONST_CHAR::bPKLESS || pChar->m_nChannel != 0) 
		{
			GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
			NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}

		if ( !GLPVPSchoolWarsAgent::GetInstance().IsBattle() )
		{
			GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
			NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}
	}

	/*pvp capture the flag, Juver, 2018/01/31 */
	if ( pNextLandMan->m_bPVPCaptureTheFlagMap )
	{
		if ( GLCONST_CHAR::bPKLESS || pChar->m_nChannel != 0) 
		{
			GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
			NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}

		if ( !GLPVPCaptureTheFlagAgent::GetInstance().IsBattle() )
		{
			GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
			NetMsgFB.emFB = EMCHAR_GATEOUT_HOLD;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}
	}

	//	Note : 목표 게이트.
	//
	DxLandGateMan* pNextGateMan = pNextLandMan->GetLandGateMan ();
	PDXLANDGATE pNextGate = pNextGateMan->FindLandGate ( pCurGate->GetToGateID() );
	if ( !pNextGate )
	{
		CDebugSet::ToLogFile( "EMCHAR_GATEOUT_TARGATEID %d, %d %d %d %d", 
			pChar->m_dwCharID, pChar->m_sCurMapID, pNetMsg->dwGateID, pCurGate->GetToMapID(), 
			pCurGate->GetToGateID() );

		GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
		NetMsgFB.emFB = EMCHAR_GATEOUT_TARGATEID;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Note : 필드 서버가 같은 맵종류 일때에는 필드 서버 변경이 필요 없음.
	//
	DWORD dwNextFieldSvr = GetFieldServer ( pCurGate->GetToMapID() );
	if ( dwNextFieldSvr==pChar->m_dwCurFieldSvr )
	{
		// Agent 서버에서 갖고 있는 케릭터 맵정보 변경
		pChar->SetCurrentField ( pChar->m_dwCurFieldSvr, pCurGate->GetToMapID() );

		//	Note : 필드 서버 변경 필요 없음. 현재 필드서버로 맵이동을 요청.
		//
		SENDTOFIELDSVR ( pChar->m_nChannel, pChar->m_dwCurFieldSvr, nmg );
	}
	//	Note : 필드 서버 변경 필요.
	//
	else
	{
		//	Note : 필드 서버가 정상인지 검사.
		//
		BOOL bSVR_OK = m_pMsgServer->IsSlotOnline ( dwNextFieldSvr );
		if ( !bSVR_OK )
		{
			GLMSG::SNETREQ_GATEOUT_FB NetMsgFB;
			NetMsgFB.emFB = EMCHAR_GATEOUT_FIELDSVR;
			SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
			return FALSE;
		}

		//	Note : 필드 서버 변경시 클라이언트 정보 변경 사항 반영.
		//
		ReActionMoveFieldSvr ( pChar->m_dwClientID, pChar->m_dwGaeaID );

		//	현재 필드 서버에 자신이 떠나갈 예정임을 통보.
		GLMSG::SNETPC_FIELDSVR_OUT NetMsg;
		SENDTOFIELD ( pChar->m_dwClientID, &NetMsg );

		//	이동할 맵 ID 저장.
		pChar->SetNextFieldSvr ( pCurGate->GetToMapID(), pCurGate->GetToGateID(), D3DXVECTOR3(FLT_MAX,FLT_MAX,FLT_MAX), dwNextFieldSvr );


		
	}

	return TRUE;
}

BOOL GLAgentServer::CheckStartMap( DWORD dwGaeaID )
{
	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )			return FALSE;

	GLAGLandMan *pLandMan=NULL;
	DWORD dwGenGate = 0;
	pLandMan = GetByMapID ( pChar->m_sStartMapID );

	if ( pLandMan && pLandMan->m_bClubDeathMatchMap )
	{
		if ( GLCONST_CHAR::bPKLESS ) return FALSE;

		if ( pChar->m_nChannel != 0 )	return FALSE;

        GLClubDeathMatch *pCDM = GLClubDeathMatchAgentMan::GetInstance().Find ( pLandMan->m_dwClubMapID );
		if ( pCDM && !pCDM->IsEnterMap( pChar->m_dwGuild ) )	return FALSE;

		GLCLUB* pClub = m_cClubMan.GetClub ( pChar->m_dwGuild );
		if ( !pClub )	return FALSE;		

		// 클럽배틀 진행중일때는 못들어간다.
		if ( pClub->GetAllBattleNum() > 0 )	return FALSE;
		if ( !pClub->IsMemberFlgCDM(pChar->m_dwCharID) )	return FALSE;
	}

	/*pvp tyranny, Juver, 2017/08/24 */
	/*school wars, Juver, 2018/01/19 */
	/*pvp capture the flag, Juver, 2018/01/31 */
	if ( pLandMan && ( pLandMan->m_bPVPTyrannyMap || pLandMan->m_bPVPSchoolWarsMap || pLandMan->m_bPVPCaptureTheFlagMap  ) )
	{
		return FALSE;
	}

	if ( !pLandMan ) 		
	{
		return FALSE;
	}
	else if ( !pLandMan->GetLandGateMan()->FindLandGate(pChar->m_dwStartGate) )
	{
		PDXLANDGATE pLandGate = pLandMan->GetLandGateMan()->GetListLandGate();
		if ( !pLandGate )	return FALSE;	//	Gate가 하나도 없는 곳엔 부활할 수 없음.
	}

	return TRUE;
}

// *****************************************************
// Desc: 부활요청 처리
// *****************************************************
BOOL GLAgentServer::MsgReqReBirth ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )			return FALSE;

	//  부활 지점으로 진입 불가시 부활 지점을 초기화한다.
	GLMSG::SNETPC_REQ_REBIRTH *pNetMsg = (GLMSG::SNETPC_REQ_REBIRTH *) nmg;

	if ( pChar->m_bDeathTimer )
	{
		//fastrevive fix
		GLMSG::SNETPC_REQ_DEATHTIMER_FB_AG NetMsgFB;
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}

	bool bStartMap = false;
	
	//	부활.
	GLAGLandMan *pLandMan=NULL;
	DWORD dwGenGate = 0;

	//	시작위치가 변경되어야 하는 경우
	if ( pNetMsg->bRegenEntryFailed || !CheckStartMap( dwGaeaID ) )
	{
		pChar->m_sStartMapID = GLCONST_CHAR::nidSTARTMAP[pChar->m_wSchool];
		pChar->m_dwStartGate = GLCONST_CHAR::dwSTARTGATE[pChar->m_wSchool];
		pChar->m_vStartPos   = D3DXVECTOR3(0.0f,0.0f,0.0f);

		SNATIVEID nidSTARTMAP = pChar->m_sStartMapID;
		DWORD dwSTARTGATE = pChar->m_dwStartGate;

		pLandMan = GetByMapID ( nidSTARTMAP );
		if ( pLandMan )
		{
			DxLandGateMan* pLandGateMan = pLandMan->GetLandGateMan();
			if ( !pLandGateMan->FindLandGate ( dwSTARTGATE ) )
			{
				DEBUGMSG_WRITE ( "(Revive) - Default gate is a failure. school %d", pChar->m_wSchool );
				return FALSE;
			}
			else
			{
				dwGenGate = dwSTARTGATE;
			}
		}
		else
		{
			DEBUGMSG_WRITE(	"GLAgentServer::MsgReqReBirth Default map is a failure. school %d",	pChar->m_wSchool );
			return FALSE;
		}
	}
	else
	{
		pLandMan = GetByMapID ( pChar->m_sStartMapID );
		
		//	여기 이외의 상황은 위에서 다 걸러진다. 
		if ( pLandMan )
		{
			if ( !pLandMan->GetLandGateMan()->FindLandGate(pChar->m_dwStartGate) ) 
			{
				PDXLANDGATE pLandGate = pLandMan->GetLandGateMan()->GetListLandGate();
				if ( pLandGate )	dwGenGate = pLandGate->GetGateID();		//	첫번째 Gate 에서 생김.
				else
				{
					//	일어날수 없는 상황
					DEBUGMSG_WRITE(	"GLAgentServer::MsgReqReBirth Error Logic No Gate" );
					return FALSE;
				}
			}
			else	dwGenGate = pChar->m_dwStartGate;
		}
		else
		{
			//	일어날수 없는 상황
			DEBUGMSG_WRITE(	"GLAgentServer::MsgReqReBirth Error Logic No LandMan" );
			return FALSE;
		}
	}

/*
	if( pNetMsg->bRegenEntryFailed )
	{
		pChar->m_sStartMapID = GLCONST_CHAR::nidSTARTMAP[pChar->m_wSchool];
		pChar->m_dwStartGate = GLCONST_CHAR::dwSTARTGATE[pChar->m_wSchool];
		pChar->m_vStartPos   = D3DXVECTOR3(0.0f,0.0f,0.0f);
	}

	//	부활.
	GLAGLandMan *pLandMan=NULL;
	DWORD dwGenGate = 0;

	//	부활할 Map과 Gate 유효성 검사.
	if ( pChar->m_sStartMapID!=NATIVEID_NULL() )
	{
		pLandMan = GetByMapID ( pChar->m_sStartMapID );

		if ( pLandMan )
		{
			DxLandGateMan* pLandGateMan = pLandMan->GetLandGateMan();
			if ( pLandGateMan->FindLandGate(pChar->m_dwStartGate)==NULL )
			{
				PDXLANDGATE pLandGate = pLandGateMan->GetListLandGate();
				if ( !pLandGate )	pLandMan = NULL;					//	Gate가 하나도 없는 곳엔 부활할 수 없음.
				else				dwGenGate = pLandGate->GetGateID();	//	첫번째 Gate 에서 생김.
			}
			else					dwGenGate = pChar->m_dwStartGate;
		}
	}
	

	if ( !pLandMan )
	{
		SNATIVEID nidSTARTMAP = GLCONST_CHAR::nidSTARTMAP[pChar->m_wSchool];
		DWORD dwSTARTGATE = GLCONST_CHAR::dwSTARTGATE[pChar->m_wSchool];

		pLandMan = GetByMapID ( nidSTARTMAP );
		if ( pLandMan )
		{
			DxLandGateMan* pLandGateMan = pLandMan->GetLandGateMan();
			if ( !pLandGateMan->FindLandGate ( dwSTARTGATE ) )
			{
				DEBUGMSG_WRITE ( "(Revive) - Default gate is a failure. school %d", pChar->m_wSchool );
				return FALSE;
			}
			else
			{
				dwGenGate = dwSTARTGATE;
			}
		}
	}

	if ( !pLandMan )
	{
		DEBUGMSG_WRITE(
			"GLAgentServer::MsgReqReBirth Default map is a failure. school %d",
			pChar->m_wSchool );
		return FALSE;
	}
*/
	SNATIVEID sBirthMapID = pLandMan->GetMapID ();

	DWORD dwNextFieldSvr = GetFieldServer ( sBirthMapID );
	if ( dwNextFieldSvr==pChar->m_dwCurFieldSvr )
	{
		pChar->SetCurrentField ( pChar->m_dwCurFieldSvr, sBirthMapID );

		//	Note : 필드 서버 변경이 필요 없을때. 현재 필드서버로 부활을 요청.
		//
		SENDTOFIELD ( dwClientID, nmg );
	}
	else
	{
		//	Note : 필드 서버의 변경이 필요할 경우.
		//

		//	Note : 필드 서버 변경시 클라이언트 정보 변경 사항 반영.
		//
		ReActionMoveFieldSvr ( dwClientID, dwGaeaID );


		//	현제 필드 서버에 자신이 다른 곳에서 부활할 예정임을 통보.
		GLMSG::SNETPC_REBIRTH_OUT NetMsg;
		SENDTOFIELD ( dwClientID, &NetMsg );

		//	이동할 맵 ID 저장.
		pChar->SetNextFieldSvr ( sBirthMapID, dwGenGate, D3DXVECTOR3(FLT_MAX,FLT_MAX,FLT_MAX), dwNextFieldSvr );
	}

	return TRUE;
}

BOOL GLAgentServer::MsgReqReGenGate ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_REQ_REGEN_GATE_FB *pNetMsg = (GLMSG::SNETPC_REQ_REGEN_GATE_FB *)nmg;

	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )			return FALSE;

	//	Note : 변경된 스타트 멥, 스타트 게이트의 정보를 적용.
	//
	pChar->m_sStartMapID = pNetMsg->sMapID;
	pChar->m_dwStartGate = pNetMsg->dwGateID;

	SENDTOCLIENT ( pChar->m_dwClientID, nmg );

	return TRUE;
}

// *****************************************************
// Desc: 필드서버에 접속정보 전송(필드서버가 바뀔경우)
// *****************************************************
BOOL GLAgentServer::MsgFieldSvrOutFb ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	if( dwGaeaID < 0 || dwGaeaID >= m_dwMaxClient ) 
		return FALSE;

	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )			return FALSE;

	if ( pChar->m_sNextMapID==SNATIVEID(MAXLANDMID,MAXLANDSID) )	return FALSE;

	//	Note : 현재 필드 서버 및 맵 ID 지정.
	pChar->SetCurrentField ( pChar->m_dwNextFieldSvr, pChar->m_sNextMapID );

	//	Note : 맵에 새로 진입하기 위해서 Field서버에 메시지 송신.
	//
	NET_GAME_JOIN_FIELDSVR NetJoinField;
	NetJoinField.emType = EMJOINTYPE_MOVEMAP;
	
	NetJoinField.dwSlotAgentClient = pChar->m_dwClientID;
	StringCchCopy ( NetJoinField.szUID, DAUM_MAX_UID_LENGTH+1, pChar->m_szUserName );
	NetJoinField.nUserNum = pChar->m_dwUserID;
	NetJoinField.dwUserLvl = pChar->m_dwUserLvl;
	NetJoinField.nChaNum = pChar->m_dwCharID;
	NetJoinField.dwGaeaID = pChar->m_dwGaeaID;
	NetJoinField.tPREMIUM = pChar->m_tPREMIUM;
	NetJoinField.tCHATBLOCK = pChar->m_tCHATBLOCK;

	NetJoinField.sStartMap = pChar->m_sNextMapID;
	NetJoinField.dwStartGate = pChar->m_dwNextGateID;
	NetJoinField.vStartPos = pChar->m_vNextPos;

	NetJoinField.dwActState = pChar->m_dwActState;
	NetJoinField.bUseArmSub	= pChar->m_bUseArmSub;

	NetJoinField.dwThaiCCafeClass	= pChar->m_dwThaiCCafeClass;
	NetJoinField.nMyCCafeClass		= pChar->m_nMyCCafeClass;				// 말레이시아 PC방 이벤트

	NetJoinField.sChinaTime			= pChar->m_sChinaTime;
	NetJoinField.sEventTime			= pChar->m_sEventTime;
	NetJoinField.sVietnamGainSystem = pChar->m_sVietnamSystem;

	

	//** Add EventTime


	pChar->ResetNextFieldSvr ();

	SENDTOFIELD ( dwClientID, &NetJoinField );

	return TRUE;
}

BOOL GLAgentServer::MsgReBirthOutFb ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )			return FALSE;

	if ( pChar->m_sNextMapID==SNATIVEID(MAXLANDMID,MAXLANDSID) )	return FALSE;

	//	Note : 현재 필드 서버 및 맵 ID 지정.
	pChar->SetCurrentField ( pChar->m_dwNextFieldSvr, pChar->m_sNextMapID );

	//	Note : 맵에 새로 진입하기 위해서 Field서버에 메시지 송신.
	//
	NET_GAME_JOIN_FIELDSVR NetJoinField;
	NetJoinField.emType = EMJOINTYPE_REBIRTH;
	
	NetJoinField.dwSlotAgentClient = pChar->m_dwClientID;
	NetJoinField.nUserNum = pChar->m_dwUserID;
	StringCchCopy ( NetJoinField.szUID, DAUM_MAX_UID_LENGTH+1, pChar->m_szUserName );
	NetJoinField.dwUserLvl = pChar->m_dwUserLvl;
	NetJoinField.nChaNum = pChar->m_dwCharID;
	NetJoinField.dwGaeaID = pChar->m_dwGaeaID;
	NetJoinField.tPREMIUM = pChar->m_tPREMIUM;
	NetJoinField.tCHATBLOCK = pChar->m_tCHATBLOCK;

	NetJoinField.sStartMap = pChar->m_sNextMapID;
	NetJoinField.dwStartGate = pChar->m_dwNextGateID;
	NetJoinField.vStartPos = D3DXVECTOR3(0,0,0);

	NetJoinField.dwActState = pChar->m_dwActState;
	NetJoinField.bUseArmSub	= pChar->m_bUseArmSub;

	NetJoinField.dwThaiCCafeClass	= pChar->m_dwThaiCCafeClass;
	NetJoinField.nMyCCafeClass		= pChar->m_nMyCCafeClass;					// 말레이시아 PC방 이벤트

	NetJoinField.sChinaTime			= pChar->m_sChinaTime;
	NetJoinField.sEventTime			= pChar->m_sEventTime;
	NetJoinField.sVietnamGainSystem = pChar->m_sVietnamSystem;

	//** Add EventTime

	pChar->ResetNextFieldSvr ();

	SENDTOFIELD ( dwClientID, &NetJoinField );

	return TRUE;
}
// *****************************************************
// Desc: 레벨 업시 현재 맵에 머물러 있지 않아야 하는 경우
// *****************************************************
BOOL GLAgentServer::MsgLvUpMustLeaveMap( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )			return FALSE;

	GLMSG::SNETPC_REQ_MUST_LEAVE_MAP_AG *pNetMsg = (GLMSG::SNETPC_REQ_MUST_LEAVE_MAP_AG *) nmg;

	//	Note : 목표 맵.
	//
	GLAGLandMan* pNextLandMan = GetByMapID ( pNetMsg->sMAPID );
	if ( !pNextLandMan )
	{
		GLMSG::SNETPC_REQ_MUST_LEAVE_MAP_FB NetMsgFB;
		NetMsgFB.emFB = EMREQ_MUST_LEAVE_MAP_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Note : 이동 위치 정보 유효성 검사.
	//
	if ( pNetMsg->dwGATEID==UINT_MAX && pNetMsg->vPOS==D3DXVECTOR3(FLT_MAX,FLT_MAX,FLT_MAX) )
	{
		GLMSG::SNETPC_REQ_MUST_LEAVE_MAP_FB NetMsgFB;
		NetMsgFB.emFB = EMREQ_MUST_LEAVE_MAP_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	// 맵이 있는지
	SMAPNODE* pMapNode = m_sMapList.FindMapNode ( pNetMsg->sMAPID );
	if ( !pMapNode )
	{
		GLMSG::SNETPC_REQ_MUST_LEAVE_MAP_FB NetMsgFB;
		NetMsgFB.emFB = EMREQ_MUST_LEAVE_MAP_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	DWORD dwNextFieldSvr = pMapNode->dwFieldSID;

	// 필드서버를 옮겨야 할 경우
	{
		//	Note : 필드서버가 정상인지 검사.
		//
		BOOL bSVR_OK = m_pMsgServer->IsSlotOnline ( dwNextFieldSvr );
		if ( !bSVR_OK )
		{
			GLMSG::SNETPC_REQ_MUST_LEAVE_MAP_FB NetMsgFB;
			NetMsgFB.emFB = EMREQ_MUST_LEAVE_MAP_FB_FAIL;
			SENDTOCLIENT ( dwClientID, &NetMsgFB );
			return FALSE;
		}

		//	Note : 필드 서버 변경시 클라이언트 정보 변경 사항 반영.
		//
		ReActionMoveFieldSvr ( dwClientID, dwGaeaID );

		//	현재 필드 서버에 자신이 떠나갈 예정임을 통보.
		GLMSG::SNETPC_FIELDSVR_OUT NetMsg;
		SENDTOFIELD ( pChar->m_dwClientID, &NetMsg );

		//	이동할 맵 ID 저장.
		pChar->SetNextFieldSvr ( pNetMsg->sMAPID, pNetMsg->dwGATEID, pNetMsg->vPOS, dwNextFieldSvr );

	}

	return TRUE;
}


// *****************************************************
// Desc: 소환요청 처리 (다른 필드서버일 경우)
// *****************************************************
BOOL GLAgentServer::MsgReCallAg ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )			return FALSE;

	GLMSG::SNETPC_REQ_RECALL_AG *pNetMsg = (GLMSG::SNETPC_REQ_RECALL_AG *) nmg;

	//	Note : 목표 맵.
	//
	GLAGLandMan* pNextLandMan = GetByMapID ( pNetMsg->sMAPID );
	if ( !pNextLandMan )
	{
		GLMSG::SNETPC_REQ_RECALL_FB NetMsgFB;
		NetMsgFB.emFB = EMREQ_RECALL_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Note : 이동 위치 정보 유효성 검사.
	//
	if ( pNetMsg->dwGATEID==UINT_MAX && pNetMsg->vPOS==D3DXVECTOR3(FLT_MAX,FLT_MAX,FLT_MAX) )
	{
		GLMSG::SNETPC_REQ_RECALL_FB NetMsgFB;
		NetMsgFB.emFB = EMREQ_RECALL_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	// 맵이 있는지
	SMAPNODE* pMapNode = m_sMapList.FindMapNode ( pNetMsg->sMAPID );
	if ( !pMapNode )
	{
		GLMSG::SNETPC_REQ_RECALL_FB NetMsgFB;
		NetMsgFB.emFB = EMREQ_RECALL_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	DWORD dwNextFieldSvr = pMapNode->dwFieldSID;

	// 필드서버를 옮겨야 할 경우
	{
		//	Note : 필드서버가 정상인지 검사.
		//
		BOOL bSVR_OK = m_pMsgServer->IsSlotOnline ( dwNextFieldSvr );
		if ( !bSVR_OK )
		{
			GLMSG::SNETPC_REQ_RECALL_FB NetMsgFB;
			NetMsgFB.emFB = EMREQ_RECALL_FB_FAIL;
			SENDTOCLIENT ( dwClientID, &NetMsgFB );
			return FALSE;
		}

		//	Note : 필드 서버 변경시 클라이언트 정보 변경 사항 반영.
		//
		ReActionMoveFieldSvr ( dwClientID, dwGaeaID );

		//	현재 필드 서버에 자신이 떠나갈 예정임을 통보.
		GLMSG::SNETPC_FIELDSVR_OUT NetMsg;
		SENDTOFIELD ( pChar->m_dwClientID, &NetMsg );

		//	이동할 맵 ID 저장.
		pChar->SetNextFieldSvr ( pNetMsg->sMAPID, pNetMsg->dwGATEID, pNetMsg->vPOS, dwNextFieldSvr );

	}

	return TRUE;
}

BOOL GLAgentServer::MsgTeleportAg ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )			return FALSE;

	GLMSG::SNETPC_REQ_TELEPORT_AG *pNetMsg = (GLMSG::SNETPC_REQ_TELEPORT_AG *) nmg;

	//	Note : 목표 맵.
	//
	GLAGLandMan* pNextLandMan = GetByMapID ( pNetMsg->sMAPID );
	if ( !pNextLandMan )
	{
		GLMSG::SNETPC_REQ_TELEPORT_FB NetMsgFB;
		NetMsgFB.emFB = EMREQ_TELEPORT_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Note : 이동 위치 정보 유효성 검사.
	//
	if ( pNetMsg->vPOS==D3DXVECTOR3(FLT_MAX,FLT_MAX,FLT_MAX) )
	{
		GLMSG::SNETPC_REQ_TELEPORT_FB NetMsgFB;
		NetMsgFB.emFB = EMREQ_TELEPORT_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	// 맵이 있는지
	SMAPNODE* pMapNode = m_sMapList.FindMapNode ( pNetMsg->sMAPID );
	if ( !pMapNode )
	{
		GLMSG::SNETPC_REQ_TELEPORT_FB NetMsgFB;
		NetMsgFB.emFB = EMREQ_TELEPORT_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	DWORD dwNextFieldSvr = pMapNode->dwFieldSID;

	// 필드서버를 옮겨야 할 경우
	{
		//	Note : 필드서버가 정상인지 검사.
		//
		BOOL bSVR_OK = m_pMsgServer->IsSlotOnline ( dwNextFieldSvr );
		if ( !bSVR_OK )
		{
			GLMSG::SNETPC_REQ_TELEPORT_FB NetMsgFB;
			NetMsgFB.emFB = EMREQ_TELEPORT_FB_FAIL;
			SENDTOCLIENT ( dwClientID, &NetMsgFB );
			return FALSE;
		}

		//	Note : 필드 서버 변경시 클라이언트 정보 변경 사항 반영.
		//
		ReActionMoveFieldSvr ( dwClientID, dwGaeaID );

		//	현재 필드 서버에 자신이 떠나갈 예정임을 통보.
		GLMSG::SNETPC_FIELDSVR_OUT NetMsg;
		SENDTOFIELD ( pChar->m_dwClientID, &NetMsg );

		//	이동할 맵 ID 저장.
		pChar->SetNextFieldSvr ( pNetMsg->sMAPID, UINT_MAX, pNetMsg->vPOS, dwNextFieldSvr );

	}

	return TRUE;

}

BOOL GLAgentServer::MsgPartyLure ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_PARTY_LURE* pNetMsg = (GLMSG::SNET_PARTY_LURE*) nmg;
	
	PGLCHARAG pCharMaster = GetChar ( dwGaeaID );
	if ( !pCharMaster )							return FALSE;
	DWORD dwPartyID = pCharMaster->m_dwPartyID;

	PGLCHARAG pCharTar = GetChar ( pNetMsg->dwGaeaID );
	if ( !pCharTar )							return FALSE;

	if ( !pNetMsg->sOption.VALID_OPT() )
	{
		GLMSG::SNET_PARTY_LURE_FB NetMsgFB;
		NetMsgFB.dwGaeaID = dwGaeaID;
		NetMsgFB.emAnswer = EMPARTY_LURE_OPT;

		SENDTOCLIENT ( pCharMaster->m_dwClientID, &NetMsgFB );

		return FALSE;
	}

	//	Note : 대상이 이미 파티에 가입되어 있음.
	//
	if ( pCharTar->m_dwPartyID!=PARTY_NULL )
	{
		GLMSG::SNET_PARTY_LURE_FB NetMsgFB;
		NetMsgFB.dwGaeaID = dwGaeaID;
		NetMsgFB.emAnswer = EMPARTY_LURE_OTHER;

		SENDTOCLIENT ( pCharMaster->m_dwClientID, &NetMsgFB );

		return FALSE;
	}

	GLClubMan& cClubMan = GLAgentServer::GetInstance().GetClubMan();

	GLCLUB *pCLUB = cClubMan.GetClub ( pCharTar->m_dwGuild );
	GLCLUB *pCLUB_M = cClubMan.GetClub ( pCharMaster->m_dwGuild );

	if ( pCLUB )
	{
		bool bClubBattle = false;
		// 파티가 없을경우 ( 자신만 체크 ) 				
		if ( dwPartyID==PARTY_NULL )
		{
			if ( pCLUB->IsBattle( pCharMaster->m_dwGuild ) ) bClubBattle = true;
			if ( pCLUB_M && pCLUB->IsBattleAlliance( pCLUB_M->m_dwAlliance ) ) bClubBattle = true;
		}
		//	있을경우 모든 멤버랑 체크
		else
		{		
			GLPARTY* pParty = m_cPartyMan.GetParty ( dwPartyID );
			
			if( pParty && pParty->ISVAILD() )
			{
				GLPARTY::MEMBER_ITER iter = pParty->m_cMEMBER.begin();
				GLPARTY::MEMBER_ITER iter_end = pParty->m_cMEMBER.end();
				PGLCHARAG pMemChar = NULL;

				for ( ; iter!=iter_end; ++iter )
				{
					pMemChar = GLAgentServer::GetInstance().GetChar ( (*iter) );
					if ( !pMemChar ) continue;
			
					if ( pCLUB->IsBattle(pMemChar->m_dwGuild) )
					{
						bClubBattle = true;
						break;
					}
					
					GLCLUB* pClub_Mem = cClubMan.GetClub( pMemChar->m_dwGuild );

					if( pClub_Mem && pCLUB->IsBattleAlliance( pClub_Mem->m_dwAlliance ))
					{
						bClubBattle = true;
						break;
					}
				}
			}
		}	

		if ( bClubBattle ) 
		{
			GLMSG::SNET_PARTY_LURE_FB NetMsgFB;
			NetMsgFB.dwGaeaID = dwGaeaID;
			NetMsgFB.emAnswer = EMPARTY_LURE_CLUBBATTLE;

			SENDTOCLIENT ( pCharMaster->m_dwClientID, &NetMsgFB );

			return FALSE;
		}
	}

	/*pvp capture the flag, Juver, 2018/02/03 */
	BOOL bCaptureTheFlag = FALSE;
	GLAGLandMan* pLandMan = GetByMapID ( pCharMaster->m_sCurMapID );
	if ( pLandMan )
		bCaptureTheFlag = pLandMan->m_bPVPCaptureTheFlagMap;

	if ( bCaptureTheFlag && pCharMaster && pCharTar )
	{
		if ( pCharMaster->m_wCaptureTheFlagTeam != pCharTar->m_wCaptureTheFlagTeam )
		{
			GLMSG::SNET_PARTY_LURE_FB NetMsgFB;
			NetMsgFB.dwGaeaID = dwGaeaID;
			NetMsgFB.emAnswer = EMPARTY_LURE_OTHER_TEAM;
			SENDTOCLIENT ( pCharMaster->m_dwClientID, &NetMsgFB );
			return FALSE;
		}
	}

	//	Note : 다른 학원생과 파티 결성 제한 옵션이 있을 경우 제한함.
	if ( !GLCONST_CHAR::bPARTY_2OTHERSCHOOL && !bCaptureTheFlag )
	{
		if ( pCharTar->m_wSchool!=pCharMaster->m_wSchool )
		{
			GLMSG::SNET_PARTY_LURE_FB NetMsgFB;
			NetMsgFB.dwGaeaID = dwGaeaID;
			NetMsgFB.emAnswer = EMPARTY_LURE_OTHERSCHOOL;

			SENDTOCLIENT ( pCharMaster->m_dwClientID, &NetMsgFB );

			return FALSE;
		}
	}

	//	Note : 아직 자신의 파티가 만들어 지지 않았을 경우. 대상에게 파티 참가 요청.
	//
	if ( dwPartyID==PARTY_NULL )
	{
		pCharMaster->m_sReqPartyOpt = pNetMsg->sOption;

		GLMSG::SNET_PARTY_LURE_TAR NetMsgLureTar;
		NetMsgLureTar.dwMasterID = dwGaeaID;
		NetMsgLureTar.dwPartyID = PARTY_NULL;
		NetMsgLureTar.sOption = pCharMaster->m_sReqPartyOpt;

		SENDTOCLIENT ( pCharTar->m_dwClientID, &NetMsgLureTar );

		//ptcheatfix
		pCharMaster->m_mapPartyReq[pCharTar->m_dwCharID] = pCharTar->m_dwCharID;

		return TRUE;
	}

	GLPARTY* pParty = m_cPartyMan.GetParty ( dwPartyID );
	if ( !pParty )								return FALSE;
	if ( !pParty->ISMASTER(dwGaeaID) )			return FALSE;

	if ( pParty->ISFULL() )
	{
		GLMSG::SNET_PARTY_LURE_FB NetMsgFB;
		NetMsgFB.dwGaeaID = dwGaeaID;
		NetMsgFB.emAnswer = EMPARTY_LURE_FULL;

		SENDTOCLIENT ( pCharMaster->m_dwClientID, &NetMsgFB );

		return FALSE;
	}

	//ptcheatfix
	pCharMaster->m_mapPartyReq[pCharTar->m_dwCharID] = pCharTar->m_dwCharID;

	//	Note : 대상자를 모집 대상에 등록함.
	//
	pParty->PREADDMEMBER ( pCharTar->m_dwGaeaID );
    
	//	Note : 대상에게 파티 참가 요청.
	//
	GLMSG::SNET_PARTY_LURE_TAR NetMsgLureTar;
	NetMsgLureTar.dwMasterID = dwGaeaID;
	NetMsgLureTar.dwPartyID = dwPartyID;
	NetMsgLureTar.sOption = pCharMaster->m_sReqPartyOpt;

	SENDTOCLIENT ( pCharTar->m_dwClientID, &NetMsgLureTar );

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
	if( pCharMaster->m_bTracingUser )
	{
		CDebugSet::ToTracingFile( pCharMaster->m_szUserName, "Party Request, [%s][%s], Target Char: [%s][%s]",
								  pCharMaster->m_szUserName, pCharMaster->m_szName, pCharTar->m_szUserName, pCharTar->m_szName );
	}
#endif

	return TRUE;
}

BOOL GLAgentServer::MsgPartyLureTarAns ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_PARTY_LURE_TAR_ANS* pNetMsg = (GLMSG::SNET_PARTY_LURE_TAR_ANS*) nmg;

	PGLCHARAG pCharMaster = GetChar ( pNetMsg->dwMasterID );
	if ( !pCharMaster )											return FALSE;
	
	// 파티가 최초로 구성될때
	if ( pNetMsg->dwPartyID == PARTY_NULL && pCharMaster->GetPartyID() == PARTY_NULL )
	{
		PGLCHARAG pCharTar = GetChar ( dwGaeaID );
		if ( !pCharTar )										return FALSE;
		if ( pCharTar->GetPartyID()!=PARTY_NULL )				return FALSE;

		if ( pNetMsg->emAnswer != EMPARTY_LURE_OK )
		{
			if ( pCharMaster->m_sReqPartyOpt!=pNetMsg->sOption )
			{
				GLMSG::SNET_PARTY_LURE_FB NetMsgFB;
				NetMsgFB.dwGaeaID = dwGaeaID;
				NetMsgFB.emAnswer = EMPARTY_LURE_OPT;

				SENDTOCLIENT ( pCharMaster->GetPartyID(), &NetMsgFB );
				return FALSE;
			}

			GLMSG::SNET_PARTY_LURE_FB NetMsgFB;
			NetMsgFB.dwGaeaID = dwGaeaID;
			NetMsgFB.emAnswer = pNetMsg->emAnswer;

			SENDTOCLIENT ( pCharMaster->m_dwClientID, &NetMsgFB );

			//ptcheatfix
			GLCharAG::MAPPARTY_REQ_ITER iterParty = pCharMaster->m_mapPartyReq.find( pCharTar->m_dwCharID );
			if ( iterParty != pCharMaster->m_mapPartyReq.end() )
				pCharMaster->m_mapPartyReq.erase( iterParty );

			return FALSE;
		}

		//ptcheatfix
		GLCharAG::MAPPARTY_REQ_ITER iterParty = pCharMaster->m_mapPartyReq.find( pCharTar->m_dwCharID );
		if ( iterParty == pCharMaster->m_mapPartyReq.end() )	
		{
			if ( m_pConsoleMsg )
				m_pConsoleMsg->Write( LOG_TEXT_CONSOLE, "Party Attempt Fail Character Not Invited :%d~%s", pCharTar->m_dwCharID, pCharTar->m_szName );
			return FALSE;
		}else	pCharMaster->m_mapPartyReq.erase( iterParty );

		//	Note : 파티 생성.
		//
		GLPARTY* pParty = m_cPartyMan.NewParty ( pNetMsg->dwMasterID );
		if ( !pParty )										return FALSE;

		//	Note : 파티 옵션 지정.
		//
		pParty->m_sOPTION = pNetMsg->sOption;

		//	Note : 파티 구성원에 추가. ( 첫번쩨 구성원 )
		//
		pCharTar->SetPartyID ( pParty->m_dwPARTYID );
		pParty->ADDMEMBER ( dwGaeaID );

		//	Note : 파티 생성을 필드에 알림. ( 필드 서버는 파티 정보을 받으면 새로이 파티를 구성한다. )
		//
		GLMSG::SNET_PARTY_FNEW NetPartyInfo;
		NetPartyInfo.SETINFO ( pParty->m_dwPARTYID, pParty->m_dwMASTER );
		NetPartyInfo.sOption = pParty->m_sOPTION;

		GLPARTY::MEMBER_ITER iter = pParty->m_cMEMBER.begin();
		GLPARTY::MEMBER_ITER iter_end = pParty->m_cMEMBER.end();

		for ( ; iter!=iter_end; ++iter )
		{
			PGLCHARAG pCharMem = GetChar ( (*iter) );
			NetPartyInfo.ADDMEMBER ( GLPARTY_FNET(pCharMem->m_dwGaeaID,pCharMem->m_szName,pCharMem->m_emClass,pCharMem->m_sCurMapID) );
		}

		SENDTOALLCHANNEL ( &NetPartyInfo );

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
		if( pCharTar->m_bTracingUser )
		{
			CDebugSet::ToTracingFile( pCharTar->m_szUserName, "Create Party Answer OK, [%s][%s], Master Char: [%s][%s]",
					 				  pCharTar->m_szUserName, pCharTar->m_szName, pCharMaster->m_szUserName, pCharMaster->m_szName );
		}
#endif

		return TRUE;
	}
	// 파티원을 추가할때
	else if ( pNetMsg->dwMasterID == pCharMaster->m_dwGaeaID )
	{
		GLPARTY* pParty = m_cPartyMan.GetParty ( pCharMaster->GetPartyID() );
		if ( !pParty )									return FALSE;
		if ( pParty->ISFULL() )							return FALSE;

		// 파티가 결성되지 않은 상황에서는 예비멤버들을 추가 할수 없는
		// 경우가 발생할 수 있으므로 여기서 아래 코드를 처리하지 않는다!
		// if ( !pParty->ISPREMEMBER ( dwGaeaID ) )		return FALSE;

		//	Note : 대상자를 모집 대상에서 제외함.
		//
		pParty->PREDELMEMBER ( dwGaeaID );

		PGLCHARAG pCharTar = GetChar ( dwGaeaID );
		if ( !pCharTar )							return FALSE;

		if ( pNetMsg->emAnswer == EMPARTY_LURE_OK )
		{
			if ( pCharTar->GetPartyID()!=PARTY_NULL )	return FALSE;

			//ptcheatfix
			GLCharAG::MAPPARTY_REQ_ITER iterParty = pCharMaster->m_mapPartyReq.find( pCharTar->m_dwCharID );
			if ( iterParty == pCharMaster->m_mapPartyReq.end() )	
			{
				if ( m_pConsoleMsg )
					m_pConsoleMsg->Write( LOG_TEXT_CONSOLE, "Party Attempt Fail Character Not Invited :%d~%s", pCharTar->m_dwCharID, pCharTar->m_szName );

				return FALSE;
			}

			//	Note : 파티 구성원에 추가.
			//
			pCharTar->SetPartyID ( pCharMaster->GetPartyID() );
			pParty->ADDMEMBER ( dwGaeaID );

			//	Note : 필드 서버에 파티원이 추가됨을 알림.
			//
			GLMSG::SNET_PARTY_ADD NetPartyAdd;
			NetPartyAdd.dwPartyID = pCharMaster->GetPartyID();
			NetPartyAdd.sPartyInfo = GLPARTY_FNET(pCharTar->m_dwGaeaID,pCharTar->m_szName,pCharTar->m_emClass,pCharTar->m_sCurMapID);

			SENDTOALLCHANNEL ( &NetPartyAdd );

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
			if( pCharTar->m_bTracingUser )
			{
				CDebugSet::ToTracingFile( pCharTar->m_szUserName, "Add Party Answer OK, [%s][%s], Master Char: [%s][%s]",
					pCharTar->m_szUserName, pCharTar->m_szName, pCharMaster->m_szUserName, pCharMaster->m_szName );
			}
#endif
		}

		//ptcheatfix
		GLCharAG::MAPPARTY_REQ_ITER iterParty = pCharMaster->m_mapPartyReq.find( pCharTar->m_dwCharID );
		if ( iterParty != pCharMaster->m_mapPartyReq.end() )
			pCharMaster->m_mapPartyReq.erase( iterParty );

		//	Note : 파티 모집 요청 응답을 master에게 알림.
		//
		GLMSG::SNET_PARTY_LURE_FB NetMsgFB;
		NetMsgFB.dwGaeaID = dwGaeaID;
		NetMsgFB.emAnswer = pNetMsg->emAnswer;

		SENDTOCLIENT ( pCharMaster->m_dwClientID, &NetMsgFB );


	}

	

	return TRUE;
}

BOOL GLAgentServer::MsgPartySecede ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_PARTY_SECEDE* pNetMsg = (GLMSG::SNET_PARTY_SECEDE*) nmg;

	GLPARTY* pParty = m_cPartyMan.GetParty ( pNetMsg->dwPartyID );
	if ( !pParty )									return FALSE;
	if ( !pParty->ISMEMBER(pNetMsg->dwGaeaID) )		return FALSE;

	//	Note : 자신 이외의 사람이 탈퇴될 경우는 파티장의 권한이 있어야 가능.
	//
	if ( ( pNetMsg->dwGaeaID != dwGaeaID ) && !pParty->ISMASTER(dwGaeaID) )		return FALSE;

	PGLCHARAG pCharTar = GetChar ( pNetMsg->dwGaeaID );
	if ( !pCharTar )						return FALSE;

	//	Note : 파티원이 탈퇴 함을 파티원들에게 알림.
	//
	GLMSG::SNET_PARTY_DEL NetMsgDel;
	NetMsgDel.dwPartyID = pNetMsg->dwPartyID;
	NetMsgDel.dwDelMember = pNetMsg->dwGaeaID;

	SENDTOPARTY ( pNetMsg->dwPartyID, (NET_MSG_GENERIC*) &NetMsgDel );

	//	Note : 파티원이 탈퇴 함을 필드 서버에 알림.
	//
	SENDTOALLCHANNEL ( &NetMsgDel );

	//	Note : 파티 탈퇴 처리.
	//
	pCharTar->ReSetPartyID ();
	pParty->DELMEMBER ( pNetMsg->dwGaeaID );

	return TRUE;
}

BOOL GLAgentServer::MsgPartyAuthority( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_PARTY_AUTHORITY* pNetMsg = ( GLMSG::SNET_PARTY_AUTHORITY* ) nmg;

	GLPARTY* pParty = m_cPartyMan.GetParty ( pNetMsg->dwPartyID );
	if ( !pParty )										return FALSE;
	if ( !pParty->ISMEMBER( pNetMsg->dwGaeaID ) )		return FALSE;

	if ( ( pNetMsg->dwGaeaID != dwGaeaID ) && !pParty->ISMASTER( dwGaeaID ) )		return FALSE;

	PGLCHARAG pCharTar = GetChar ( pNetMsg->dwGaeaID );
	if ( !pCharTar )						return FALSE;

	//	Note : 파티장이 바뀐 것을 파티원들에게 알림.
	//
	GLMSG::SNET_PARTY_AUTHORITY NetMsgAuth;
	NetMsgAuth.dwPartyID = pNetMsg->dwPartyID;
	NetMsgAuth.dwGaeaID = pNetMsg->dwGaeaID;

	SENDTOPARTY ( pNetMsg->dwPartyID, ( NET_MSG_GENERIC* ) &NetMsgAuth );

	//	Note : 파티장이 바뀐 것을을 필드 서버에 알림.
	//
	SENDTOALLCHANNEL ( &NetMsgAuth );

	pParty->CHANGEMASTER( pNetMsg->dwGaeaID );

	return TRUE;
}

BOOL GLAgentServer::MsgPartyDissolve ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_PARTY_DISSOLVE* pNetMsg = (GLMSG::SNET_PARTY_DISSOLVE*) nmg;
	
	GLPARTY* pParty = m_cPartyMan.GetParty ( pNetMsg->dwPartyID );
	if ( !pParty )									return FALSE;
	if ( !pParty->ISMASTER(dwGaeaID) )				return FALSE;
	
	//	Note : 파티를 해체 한다.
	//
	m_cPartyMan.DelParty ( pNetMsg->dwPartyID );

	return TRUE;
}

BOOL GLAgentServer::MsgReqGateOutFB ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETREQ_GATEOUT_FB *pNetMsg = (GLMSG::SNETREQ_GATEOUT_FB *) nmg;

	//	Note : 클라이언트에 전송한다.
	SENDTOCLIENT ( dwClientID, nmg );

	return TRUE;
}

BOOL GLAgentServer::MsgReqReBirthFB ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_REQ_REBIRTH_FB *pNetMsg = (GLMSG::SNETPC_REQ_REBIRTH_FB *) nmg;

	//	Note : 클라이언트에 전송한다.
	SENDTOCLIENT ( dwClientID, nmg );

	return TRUE;
}

BOOL GLAgentServer::MsgReqCreateInstantMapFB ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETREQ_CREATE_INSTANT_MAP_FB *pNetMsg = (GLMSG::SNETREQ_CREATE_INSTANT_MAP_FB *) nmg;

	//	Note : 클라이언트에 전송한다.
	SENDTOCLIENT ( dwClientID, nmg );

	return TRUE;
}



BOOL GLAgentServer::MsgReqConfront ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_REQ_CONFRONT *pNetMsg = (GLMSG::SNETPC_REQ_CONFRONT *) nmg;

	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )		return FALSE;

	GLAGLandMan* pLandMan = GetByMapID ( pMyChar->m_sCurMapID );
	if ( !pLandMan )
	{
		//	Note : 일반 오류.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pLandMan->m_bGuidBattleMap )
	{
		//	Note : 일반 오류.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pLandMan->m_bClubDeathMatchMap )
	{
		//	Note : 일반 오류.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	/*pvp tyranny, Juver, 2017/08/24 */
	if ( pLandMan->m_bPVPTyrannyMap )
	{
		//	Note : 일반 오류.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	/*school wars, Juver, 2018/01/19 */
	if ( pLandMan->m_bPVPSchoolWarsMap )
	{
		//	Note : 일반 오류.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	/*pvp capture the flag, Juver, 2018/01/31 */
	if ( pLandMan->m_bPVPCaptureTheFlagMap )
	{
		//	Note : 일반 오류.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	PGLCHARAG pTarChar = GetChar ( pNetMsg->dwID );
	if ( !pTarChar )
	{
		//	Note : 일반 오류.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( !pNetMsg->sOption.VALID_OPT() )
	{
		//	Note : 일반 오류.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_OPTION;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pLandMan->IsPeaceZone() )
	{
		//	Note : 평화 지역에서는 대련 불가.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_PEACE;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	DWORD dwMY_PARTYID = pMyChar->GetPartyID();
	if ( PARTY_NULL!=dwMY_PARTYID )
	{
		GLPARTY* pPARTY = m_cPartyMan.GetParty ( dwMY_PARTYID );
		if ( pPARTY && pPARTY->ISMEMBER(pTarChar->m_dwGaeaID) )
		{
			//	Note : 같은 파티 멤버 끼리는 대련 불가.
			GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
			NetMsgFB.emFB = EMCONFRONT_PARTY;
			SENDTOCLIENT ( dwClientID, &NetMsgFB );
		}
	}

	const SCONFTING &sMYCONFTING = pMyChar->GETCONFRONTING();
	if ( sMYCONFTING.IsCONFRONTING() )
	{
		//	Note : (자신이) 이미 대련 중입니다.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_ALREADY_ME;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	const SCONFTING &sTARCONFTING = pTarChar->GETCONFRONTING ();
	if ( sTARCONFTING.IsCONFRONTING() )
	{
		//	Note : (상대방이) 이미 대련 중입니다.
		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emFB = EMCONFRONT_ALREADY_TAR;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}
	
	switch ( pNetMsg->emTYPE )
	{
	case EMCONFT_ONE:
		{
			//	Note : 대련 정보는 "대련" 종료시 양자 모두 로그를 남기므로 GaeaID 하나만 확인하면 중복 대련 체크가 가능.
			//
			DWORD dwCHARID = pMyChar->m_dwCharID;
			const SCONFT_LOG* pCONFT = pTarChar->GETCONFRONT_LOG ( dwCHARID );
			if ( pCONFT )
			{
				//	Note : 일정 시간안에 지정횟수 이상 대련 방지.
				if ( pCONFT->dwCOUNT >= GLCONST_CHAR::dwCONFRONT_MAX )
				{
					GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
					NetMsgFB.emFB = EMCONFRONT_MAX;
					NetMsgFB.fTIME = GLCONST_CHAR::fCONFRONT_RESET - pCONFT->fTIMER;	//	대련 가능에 남은시간 : <연속대련방지>리셋시간 - 경과시간.
					SENDTOCLIENT ( dwClientID, &NetMsgFB );
					return FALSE;
				}

				//	Note : 종전 대련후 일정시간 경과 이전에 대련 방지.
				if ( pCONFT->fTIMER < GLCONST_CHAR::fCONFRONT_ELAP )
				{
					GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
					NetMsgFB.emFB = EMCONFRONT_ELAP;
					NetMsgFB.fTIME = GLCONST_CHAR::fCONFRONT_ELAP - pCONFT->fTIMER;	//	대련 가능에 남은시간 : <연속대련방지>리셋시간 - 경과시간.
					SENDTOCLIENT ( dwClientID, &NetMsgFB );
					return FALSE;
				}
			}

			//	Note : 대련 오청한 정보 저장.
			//
			pMyChar->m_sREQCONFTING.RESET();
			pMyChar->m_sREQCONFTING.emTYPE = EMCONFT_ONE;
			pMyChar->m_sREQCONFTING.dwTAR_ID = pTarChar->m_dwGaeaID;
			pMyChar->m_sREQCONFTING.sOption = pNetMsg->sOption;

			//	Note : 대련 (상대방에게) 요청.
			//
			GLMSG::SNETPC_REQ_CONFRONT_TAR NetMsgTAR;
			NetMsgTAR.dwID = dwGaeaID;	// 자신의 ID.
			NetMsgTAR.sOption = pNetMsg->sOption;
			SENDTOCLIENT ( pTarChar->m_dwClientID, &NetMsgTAR );


#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
			if( pMyChar->m_bTracingUser )
			{
				CDebugSet::ToTracingFile( pMyChar->m_szUserName, "Confront1 Request, [%s][%s], Target Char: [%s][%s]",
										  pMyChar->m_szUserName, pMyChar->m_szName, pTarChar->m_szUserName, pTarChar->m_szName );
			}
#endif
		}
		break;

	case EMCONFT_PARTY:
		{
			DWORD dwMyPartyID = pMyChar->GetPartyID();
			DWORD dwTarPartyID = pTarChar->GetPartyID();
		
			if ( dwMyPartyID==GAEAID_NULL )
			{
				//	Note : (자신이) 파티를 결성하지 않았습니다.
				GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
				NetMsgFB.emFB = EMCONFRONT_MENOTPARTY;
				SENDTOCLIENT ( dwClientID, &NetMsgFB );
				return FALSE;
			}

			GLPARTY *pMyParty = m_cPartyMan.GetParty(dwMyPartyID);
			if ( NULL==pMyParty )
			{
				//	Note : (자신이) 파티를 결성하지 않았습니다.
				GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
				NetMsgFB.emFB = EMCONFRONT_MENOTPARTY;
				SENDTOCLIENT ( dwClientID, &NetMsgFB );
				return FALSE;
			}

			if ( FALSE==pMyParty->ISMASTER(pMyChar->m_dwGaeaID) )
			{
				//	Note : (자신이) 파티 마스터가 아닙니다.
				GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
				NetMsgFB.emFB = EMCONFRONT_MENOTMASTER;
				SENDTOCLIENT ( dwClientID, &NetMsgFB );
				return FALSE;
			}

			if ( dwTarPartyID==GAEAID_NULL )
			{
				//	Note : (상대방이) 파티를 결성하지 않았습니다.
				GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
				NetMsgFB.emFB = EMCONFRONT_TARMASTER;
				SENDTOCLIENT ( dwClientID, &NetMsgFB );
				return FALSE;
			}

			GLPARTY *pTarParty = m_cPartyMan.GetParty(dwTarPartyID);
			if ( NULL==pTarParty )
			{
				//	Note : (상대방이) 파티를 결성하지 않았습니다.
				GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
				NetMsgFB.emFB = EMCONFRONT_TARMASTER;
				SENDTOCLIENT ( dwClientID, &NetMsgFB );
				return FALSE;
			}

			if ( FALSE==pTarParty->ISMASTER(pTarChar->m_dwGaeaID) )
			{
				//	Note : (상대방이) 파티 마스터가 아닙니다.
				GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
				NetMsgFB.emFB = EMCONFRONT_TARMASTER;
				SENDTOCLIENT ( dwClientID, &NetMsgFB );
				return FALSE;
			}

			//	Note : 대련 오청한 정보 저장.
			//
			pMyChar->m_sREQCONFTING.RESET();
			pMyChar->m_sREQCONFTING.emTYPE = EMCONFT_PARTY;
			pMyChar->m_sREQCONFTING.dwTAR_ID = pTarChar->m_dwGaeaID;
			pMyChar->m_sREQCONFTING.sOption = pNetMsg->sOption;

			//	Note : 대련 (상대방에게) 요청.
			//
			GLMSG::SNETPC_REQ_CONFRONT_TAR NetMsgTAR;
			NetMsgTAR.emTYPE = EMCONFT_PARTY;
			NetMsgTAR.dwID = dwGaeaID;	// 자신의 ID.
			NetMsgTAR.sOption = pNetMsg->sOption;
			SENDTOCLIENT ( pTarChar->m_dwClientID, &NetMsgTAR );

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
			if( pMyChar->m_bTracingUser )
			{
				CDebugSet::ToTracingFile( pMyChar->m_szUserName, "Confront2 Request, [%s][%s], Target Char: [%s][%s]",
										  pMyChar->m_szUserName, pMyChar->m_szName, pTarChar->m_szUserName, pTarChar->m_szName );
			}
#endif
		}
		break;

	case EMCONFT_GUILD:
		{
			DWORD dwMyClubID = pMyChar->m_dwGuild;
			DWORD dwTarClubID = pTarChar->m_dwGuild;

			if ( dwTarClubID==CLUB_NULL )
			{
				//	Note : 클럽 마스터가 아닙니다.
				GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
				NetMsgFB.emFB = EMCONFRONT_FAIL;
				SENDTOCLIENT ( dwClientID, &NetMsgFB );
				return FALSE;
			}

			GLCLUB *pTarClub = m_cClubMan.GetClub ( dwTarClubID );
			if ( !pTarClub || pTarClub->m_dwMasterID!=pTarChar->m_dwCharID )
			{
				//	Note : 클럽 마스터가 아닙니다.
				GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
				NetMsgFB.emFB = EMCONFRONT_FAIL;
				SENDTOCLIENT ( dwClientID, &NetMsgFB );
				return FALSE;
			}

			if ( dwMyClubID==CLUB_NULL )
			{
				//	Note : 클럽 마스터가 아닙니다.
				GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
				NetMsgFB.emFB = EMCONFRONT_FAIL;
				SENDTOCLIENT ( dwClientID, &NetMsgFB );
				return FALSE;
			}

			GLCLUB *pMyClub = m_cClubMan.GetClub ( dwMyClubID );
			if ( !pMyClub || pMyClub->m_dwMasterID!=pMyChar->m_dwCharID )
			{
				//	Note : 클럽 마스터가 아닙니다.
				GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
				NetMsgFB.emFB = EMCONFRONT_FAIL;
				SENDTOCLIENT ( dwClientID, &NetMsgFB );
				return FALSE;
			}

			//	Note : 대련 오청한 정보 저장.
			//
			pMyChar->m_sREQCONFTING.RESET();
			pMyChar->m_sREQCONFTING.emTYPE = EMCONFT_GUILD;
			pMyChar->m_sREQCONFTING.dwTAR_ID = pTarChar->m_dwGaeaID;
			pMyChar->m_sREQCONFTING.sOption = pNetMsg->sOption;

			//	Note : 대련 (상대방에게) 요청.
			//
			GLMSG::SNETPC_REQ_CONFRONT_TAR NetMsgTAR;
			NetMsgTAR.emTYPE = EMCONFT_GUILD;
			NetMsgTAR.dwID = dwGaeaID;	// 자신의 ID.
			NetMsgTAR.sOption = pNetMsg->sOption;
			SENDTOCLIENT ( pTarChar->m_dwClientID, &NetMsgTAR );

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
			if( pMyChar->m_bTracingUser )
			{
				CDebugSet::ToTracingFile( pMyChar->m_szUserName, "Confront3 Request, [%s][%s], Target Char: [%s][%s]",
										  pMyChar->m_szUserName, pMyChar->m_szName, pTarChar->m_szUserName, pTarChar->m_szName );
			}
#endif
		}
		break;
	};

	return TRUE;
}

BOOL GLAgentServer::MsgAnsConfront ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_REQ_CONFRONT_ANS *pNetMsg = (GLMSG::SNETPC_REQ_CONFRONT_ANS *) nmg;

	// Absolute Non-PK State Check. 대련 안됨.
	if ( GLCONST_CHAR::bPKLESS ) return FALSE;

	if ( pNetMsg->emFB == EMCONFRONT_REFUSE )
	{
		PGLCHARAG pReqChar = GetChar ( pNetMsg->dwID );
		if ( !pReqChar )	return FALSE;

		GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
		NetMsgFB.emTYPE = pNetMsg->emTYPE;
		NetMsgFB.dwID = dwGaeaID;
		NetMsgFB.emFB = EMCONFRONT_REFUSE;

		SENDTOCLIENT ( pReqChar->m_dwClientID, &NetMsgFB );
	}
	else if ( pNetMsg->emFB == EMCONFRONT_AGREE )
	{
		//	대련 기본 정보.
		const EMCONFT_TYPE emTYPE = pNetMsg->emTYPE;
		const DWORD dwMY_ID = dwGaeaID;
		const DWORD dwREQ_ID = pNetMsg->dwID;
		const SCONFT_OPTION _sOption = pNetMsg->sOption;

		//	대련 상대방 ID 들.
		const DWORD dwMY_TAR = dwREQ_ID;
		const DWORD dwREQ_TAR = dwMY_ID;

		PGLCHARAG pMyChar = GetChar ( dwMY_ID );
		if ( !pMyChar )		return FALSE;

		PGLCHARAG pReqChar = GetChar ( dwREQ_ID );
		if ( !pReqChar )	return FALSE;

		const SCONFTING& sMY_CONFTING = pMyChar->GETCONFRONTING();
		const SCONFTING& sREQ_CONFTING = pReqChar->GETCONFRONTING();

		if ( sMY_CONFTING.IsCONFRONTING() || sREQ_CONFTING.IsCONFRONTING() )
			return FALSE;

		//	Note : 대련 조건 무결성 검사.
		//
		if ( emTYPE != pReqChar->m_sREQCONFTING.emTYPE )			return FALSE;
		if ( dwREQ_TAR != pReqChar->m_sREQCONFTING.dwTAR_ID )		return FALSE;
		if ( _sOption != pReqChar->m_sREQCONFTING.sOption )			return FALSE;

#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
		if( pMyChar->m_bTracingUser )
		{
			CDebugSet::ToTracingFile( pMyChar->m_szUserName, "Confront Answer OK, [%s][%s], Request Char: [%s][%s]",
									  pMyChar->m_szUserName, pMyChar->m_szName, pReqChar->m_szUserName, pReqChar->m_szName );
		}
#endif

		//	Note : 대련 시작.
		//
		switch ( emTYPE )
		{
		case EMCONFT_ONE:
			{
				SCONFT_OPTION sREQOption = _sOption;		//	A ( 요청자 )
				SCONFT_OPTION sMYOption = _sOption;			//	B ( 수락자 )
				sMYOption.fHP_RATE = _sOption.fTAR_HP_RATE;	//	( 수락자 ) 는 체력 증가율 다르개 가짐.

				pMyChar->StartConfront ( emTYPE, dwMY_TAR, sMYOption );
				pReqChar->StartConfront ( emTYPE, dwREQ_TAR, sREQOption );
			}
			break;

		case EMCONFT_PARTY:
			{
				bool bmyparty(false), breqparty(false);
				DWORD dwMyPartyID = pMyChar->GetPartyID();
				DWORD dwReqPartyID = pReqChar->GetPartyID();
			
				if ( dwMyPartyID!=GAEAID_NULL )
				{
					GLPARTY *pMyParty = m_cPartyMan.GetParty(dwMyPartyID);
					if ( pMyParty && pMyParty->ISMASTER(pMyChar->m_dwGaeaID) )
					{
						bmyparty = true;
					}
				}
				
				if ( dwReqPartyID!=GAEAID_NULL )
				{
					GLPARTY *pReqParty = m_cPartyMan.GetParty(dwReqPartyID);
					if ( pReqParty && pReqParty->ISMASTER(pReqChar->m_dwGaeaID) )
					{
						breqparty = true;
					}
				}

				//	Note : 파티 유효할때 대련 시작됨.
				//
				if ( bmyparty && breqparty )
				{
					//	Note : 대련시작전 참여 멤버 체크.
					//
					GLMSG::SNETPC_CONFRONTPTY_CHECKMBR2_FLD NetMsgCheck;
					NetMsgCheck.sMapID = pReqChar->m_sCurMapID;
					NetMsgCheck.dwPARTY_A = dwReqPartyID;
					NetMsgCheck.dwPARTY_B = dwMyPartyID;
					NetMsgCheck.sOption = _sOption;

					SENDTOFIELDSVR ( pMyChar->m_nChannel, pMyChar->m_dwCurFieldSvr, &NetMsgCheck );
				}
				else
				{
					//	Note : 파티가 아니여서 대련 불가능.
					//
					GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
					NetMsgFB.emTYPE = emTYPE;
					NetMsgFB.dwID = dwGaeaID;
					NetMsgFB.emFB = EMCONFRONT_FAIL;
					
					SENDTOCLIENT ( pMyChar->m_dwClientID, &NetMsgFB );
					SENDTOCLIENT ( pReqChar->m_dwClientID, &NetMsgFB );
				}
			}
			break;

		case EMCONFT_GUILD:
			{
				bool bmyclub(false), breqclub(false);
				DWORD dwMyClubID = pMyChar->m_dwGuild;
				DWORD dwReqClubID = pReqChar->m_dwGuild;
			
				if ( dwMyClubID!=CLUB_NULL )
				{
					GLCLUB *pCLUB = m_cClubMan.GetClub ( dwMyClubID );
					if ( pCLUB && pCLUB->m_dwMasterID==pMyChar->m_dwCharID )
					{
						bmyclub = true;
					}
				}
				
				if ( dwReqClubID!=GAEAID_NULL )
				{
					GLCLUB *pCLUB = m_cClubMan.GetClub ( dwReqClubID );
					if ( pCLUB && pCLUB->m_dwMasterID==pReqChar->m_dwCharID )
					{
						breqclub = true;
					}
				}

				//	Note : 클럽 유효할때 대련 시작됨.
				//
				if ( bmyclub && breqclub )
				{
					//	Note : 대련시작전 참여 멤버 체크.
					//
					GLMSG::SNET_CONFRONTCLB_CHECKMBR_FLD NetMsgCheck;
					NetMsgCheck.sMapID = pReqChar->m_sCurMapID;
					NetMsgCheck.dwCLUB_A = dwReqClubID;
					NetMsgCheck.dwCLUB_B = dwMyClubID;
					NetMsgCheck.sOption = _sOption;

					SENDTOFIELDSVR ( pMyChar->m_nChannel, pMyChar->m_dwCurFieldSvr, &NetMsgCheck );
				}
				else
				{
					//	Note : 클럽이 아니여서 대련 불가능.
					//
					GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
					NetMsgFB.emTYPE = emTYPE;
					NetMsgFB.dwID = dwGaeaID;
					NetMsgFB.emFB = EMCONFRONT_FAIL;
					
					SENDTOCLIENT ( pMyChar->m_dwClientID, &NetMsgFB );
					SENDTOCLIENT ( pReqChar->m_dwClientID, &NetMsgFB );
				}
			}
			break;
		};
	}

	return TRUE;
}

BOOL GLAgentServer::MsgConfrontPtyCheckMber ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNETPC_CONFRONTPTY_CHECKMBR2_AGT *pNetMsg = (GLMSG::SNETPC_CONFRONTPTY_CHECKMBR2_AGT *) nmg;
	
	StartPartyConfront ( pNetMsg->dwPARTY_A, pNetMsg->dwPARTY_B, pNetMsg->sMapID, pNetMsg->sOption,
		pNetMsg->dwPARTY_A_MEMBER, pNetMsg->dwPARTY_B_MEMBER, pNetMsg->vPosition );

	return TRUE;
}

BOOL GLAgentServer::MsgConfrontClbCheckMber ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNET_CONFRONTCLB_CHECKMBR_AGT *pNetMsg = (GLMSG::SNET_CONFRONTCLB_CHECKMBR_AGT *) nmg;
	
	GLCLUB *pClubA = m_cClubMan.GetClub(pNetMsg->dwCLUB_A);
	GLCLUB *pClubB = m_cClubMan.GetClub(pNetMsg->dwCLUB_B);
	if ( !pClubA || !pClubB )		return FALSE;

	CLUBCONFT_ITER pos, end;

	SCONFT_OPTION sOPT_A = pNetMsg->sOption;
	SCONFT_OPTION sOPT_B = pNetMsg->sOption;

	DWORD i(0);

	//	Note : A 팀 '클럽대련' 초기화.
	//
	pClubA->CLEARCONFT();

	for ( i=0; i<pNetMsg->dwCLUB_A_NUM; ++i )
	{
		DWORD dwMBR = pNetMsg->dwCLUB_A_MEMBER[i];

		PGLCHARAG pChar = GetCharID ( dwMBR );
		if ( !pChar )									continue;
		if ( pChar->m_sCurMapID != pNetMsg->sMapID )	continue;

		//	Note :클럽원의 대련정보 설정.
		//
		pChar->m_sCONFTING.RESET();
		pChar->m_sCONFTING.emTYPE = EMCONFT_GUILD;
		pChar->m_sCONFTING.dwTAR_ID = pNetMsg->dwCLUB_B;
		pChar->m_sCONFTING.sOption = sOPT_A;

		pClubA->ADDCONFT_MEMBER ( dwMBR );
	}

	if ( pClubA->GETCONFT_NUM()==0 )			return FALSE;
	
	//	Note : B 팀 '클럽대련' 초기화.
	//
	pClubB->CLEARCONFT();

	for ( i=0; i<pNetMsg->dwCLUB_B_NUM; ++i )
	{
		DWORD dwMBR = pNetMsg->dwCLUB_B_MEMBER[i];

		PGLCHARAG pChar = GetCharID ( dwMBR );
		if ( !pChar )									continue;
		if ( pChar->m_sCurMapID != pNetMsg->sMapID )	continue;

		//	Note : 클럽원의 대련정보 설정.
		//
		pChar->m_sCONFTING.RESET();
		pChar->m_sCONFTING.emTYPE = EMCONFT_GUILD;
		pChar->m_sCONFTING.dwTAR_ID = pNetMsg->dwCLUB_A;
		pChar->m_sCONFTING.sOption = sOPT_B;

		pClubB->ADDCONFT_MEMBER ( dwMBR );
	}

	if ( pClubB->GETCONFT_NUM()==0 )		return FALSE;

	//	Note : 클럽대련 정보 초기화.
	//
	pClubA->m_conftOPT = sOPT_A;
	pClubA->m_dwconftCLUB = pNetMsg->dwCLUB_B;
	pClubA->m_fconftTIMER = 0.0f;
	pClubA->SETCONFT_MEMBER();

	pClubB->m_conftOPT = sOPT_B;
	pClubB->m_dwconftCLUB = pNetMsg->dwCLUB_A;
	pClubB->m_fconftTIMER = 0.0f;
	pClubB->SETCONFT_MEMBER();

	//	Note : 필드에 대련 초기화 메시지.
	//		메시지에 전송된 my, tar school 은 사용되지 않는다.
	GLMSG::SNETPC_CONFRONTCLB_START2_FLD NetMsgFld;
	NetMsgFld.sMapID = pNetMsg->sMapID;
	NetMsgFld.dwCLUB_A= pNetMsg->dwCLUB_A;
	NetMsgFld.dwCLUB_B= pNetMsg->dwCLUB_B;
	NetMsgFld.sOption = sOPT_A;
	NetMsgFld.vPos = pNetMsg->vPosition;

	pos = pClubA->m_setConftCURRENT.begin();
	end = pClubA->m_setConftCURRENT.end();
	for ( ; pos!=end; ++pos )
	{
		NetMsgFld.dwCLUB_A_MEMBER[NetMsgFld.dwCLUB_A_NUM++] = (*pos);
	}

	pos = pClubB->m_setConftCURRENT.begin();
	end = pClubB->m_setConftCURRENT.end();
	for ( ; pos!=end; ++pos )
	{
		NetMsgFld.dwCLUB_B_MEMBER[NetMsgFld.dwCLUB_B_NUM++] = (*pos);
	}

	SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}

BOOL GLAgentServer::MsgAnsConfrontFB ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_REQ_CONFRONT_FB *pNetMsg = (GLMSG::SNETPC_REQ_CONFRONT_FB *)nmg;

	switch ( pNetMsg->emTYPE )
	{
	case EMCONFT_ONE:
		{
			PGLCHARAG pCHAR = GetChar(pNetMsg->dwID);
			if ( pCHAR->m_sCONFTING.IsCONFRONTING() )
			{
				PGLCHARAG pTAR = GetChar(pCHAR->m_sCONFTING.dwTAR_ID);
				if ( pTAR )
				{
					pTAR->m_sCONFTING.RESET();

					// Note : 클라이언트에 대련 오류 송신.
					if( pNetMsg->emFB == EMCONFRONT_FAIL )
						SENDTOCLIENT(pTAR->m_dwClientID,pNetMsg);
				}
			}

			pCHAR->m_sCONFTING.RESET();

			//	Note : 클라이언트에 대련 오류 송신.
			SENDTOCLIENT(pCHAR->m_dwClientID,pNetMsg);
		}
		break;

	case EMCONFT_PARTY:
		{
			m_cPartyMan.ConfrontFB ( (NET_MSG_GENERIC*) pNetMsg );
		}
		break;

	case EMCONFT_GUILD:
		{
			GLCLUB *pCLUB = m_cClubMan.GetClub ( pNetMsg->dwID );
			if ( pCLUB )
			{
				pCLUB->CLEARCONFT();

				CLUBMEMBERS_ITER pos = pCLUB->m_mapMembers.begin();
				CLUBMEMBERS_ITER end = pCLUB->m_mapMembers.end();
				for ( ; pos!=end; ++pos )
				{
					PGLCHARAG pCHAR = GetCharID((*pos).first);
					if ( !pCHAR )	continue;

					pCHAR->m_sCONFTING.RESET ();

					//	Note : 클라이언트에 대련 오류 송신.
					//
					GLMSG::SNETPC_REQ_CONFRONT_FB NetMsgFB;
					NetMsgFB.emFB = pNetMsg->emFB;
					SENDTOCLIENT(pCHAR->m_dwClientID,&NetMsgFB);
				}
			}
		}
		break;
	};

	return TRUE;
}

BOOL GLAgentServer::MsgConfrontEnd2Agt ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_CONFRONT_END2_AGT *pNetMsg = (GLMSG::SNETPC_CONFRONT_END2_AGT *)nmg;
	
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )					return FALSE;

	const SCONFTING &sCONFT = pMyChar->GETCONFRONTING();

	switch ( sCONFT.emTYPE )
	{
	case EMCONFT_ONE:
		{
			DWORD dwTAR_ID = sCONFT.dwTAR_ID;
			pMyChar->ResetConfrontOk ();

			PGLCHARAG pTarChar = GetChar ( dwTAR_ID );
			if ( !pTarChar )		return TRUE;
			pTarChar->ResetConfrontOk ();
		}
		break;

	case EMCONFT_PARTY:
		{
			pMyChar->ResetConfrontOk();

			GLPARTY *pParty = GetParty ( pMyChar->GetPartyID() );
			if ( !pParty )			return FALSE;
				
			pParty->DELCONFT_MEMBER ( pMyChar->m_dwGaeaID );
		}
		break;

	case EMCONFT_GUILD:
		{
			pMyChar->ResetConfrontOk();

			GLCLUB *pCLUB = m_cClubMan.GetClub ( pMyChar->m_dwGuild );
			if ( !pCLUB )			return FALSE;

			pCLUB->DELCONFT_MEMBER ( pMyChar->m_dwCharID );
		}
		break;
	};

	return TRUE;
}

BOOL GLAgentServer::MsgServerCtrlWeather ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	SENDTOALLCHANNEL ( nmg );

	return TRUE;
}

BOOL GLAgentServer::MsgServerCtrlGenHold ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	SENDTOALLCHANNEL ( nmg );

	return TRUE;
}

BOOL GLAgentServer::MsgServerCtrlTime ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNETSERVER_CTRL_TIME *pNetMsg = (GLMSG::SNETSERVER_CTRL_TIME *)nmg;
	GLPeriod::GetInstance().SetHour ( pNetMsg->dwTime );

	SENDTOALLCHANNEL ( pNetMsg );

	return TRUE;
}

BOOL GLAgentServer::MsgServerCtrlMonth ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNETSERVER_CTRL_MONTH *pNetMsg = (GLMSG::SNETSERVER_CTRL_MONTH *)nmg;

	//GLPeriod::GetInstance();

	return TRUE;
}

BOOL GLAgentServer::MsgBlockDetected ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;

	GLMSG::SNET_BLOCK_DETECTED *pNetMsg = (GLMSG::SNET_BLOCK_DETECTED*) nmg;
	if ( pMyChar->m_dwCharID == pNetMsg->dwCHARID )
	{
		std::string strTIME = CDebugSet::GetCurTime ();
		CDebugSet::ToFile ( "block_detected.txt", "%s	userid= %s (%d), charid= %s (%d), detect=%d, %s",
			strTIME.c_str(),
			pMyChar->m_szUserName,
			pMyChar->m_dwUserID, pMyChar->m_szName, pMyChar->m_dwCharID, pNetMsg->dwDETECTED, pNetMsg->szINFO );

		CLogHackProgram *pDbAction = new CLogHackProgram(
											pMyChar->m_dwUserID,
											pMyChar->m_dwCharID,
											(int) pNetMsg->dwDETECTED,
											pNetMsg->szINFO );
		GLDBMan *pDbMan = GLAgentServer::GetInstance().GetDBMan();
		if ( pDbMan )	pDbMan->AddJob ( pDbAction );
	}

	return TRUE;
}

// *****************************************************
// Desc: 케릭터 강제 접속 종료 요청 처리
// *****************************************************
BOOL GLAgentServer::MsgGmKickUser ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_KICK_USER *pNetMsg = (GLMSG::SNET_GM_KICK_USER*) nmg;

	if ( pNetMsg->bGaeaID )
	{
		for(int i=0; i < m_dwMaxClient; i++)
		{
			if( i == dwGaeaID ) continue;
			PGLCHARAG pTarChar = m_PCArray[i];
			if ( pTarChar ){
				GLMSG::SNET_GM_KICK_USER_PROC_FLD NetMsgFld;
				NetMsgFld.dwID = pTarChar->m_dwCharID;
				SENDTOALLCHANNEL ( &NetMsgFld );

				GLMSG::SNET_GM_KICK_USER_PROC NetMsgProc;
				SENDTOCLIENT ( pTarChar->m_dwClientID, &NetMsgProc );
				ReserveDropOutPC ( i );
			}
		}
	}
	else if ( pNetMsg->bName )
	{
		PGLCHARAG pTarChar = GetChar ( pNetMsg->szNAME );
		if ( !pTarChar )	return FALSE;

		GLMSG::SNET_GM_KICK_USER_PROC_FLD NetMsgFld;
		NetMsgFld.dwID = pTarChar->m_dwCharID;
		SENDTOALLCHANNEL ( &NetMsgFld );

		GLMSG::SNET_GM_KICK_USER_PROC NetMsgProc;
		SENDTOCLIENT ( pTarChar->m_dwClientID, &NetMsgProc );

		ReserveDropOutPC ( pTarChar->m_dwGaeaID );
	}
	else{
		PGLCHARAG pTarChar = GetCharID ( pNetMsg->dwID );
		if ( !pTarChar )						return FALSE;

		GLMSG::SNET_GM_KICK_USER_PROC_FLD NetMsgFld;
		NetMsgFld.dwID = pNetMsg->dwID;
		SENDTOALLCHANNEL ( &NetMsgFld );

		GLMSG::SNET_GM_KICK_USER_PROC NetMsgProc;
		SENDTOCLIENT ( pTarChar->m_dwClientID, &NetMsgProc );
		ReserveDropOutPC ( pTarChar->m_dwGaeaID );
	}

	return TRUE;
}

BOOL GLAgentServer::MsgGMMove2Gate ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pCHAR = GetChar ( dwGaeaID );
	if ( !pCHAR )						 return FALSE;
	if ( pCHAR->m_dwUserLvl < USER_GM3 ) return FALSE;

	GLMSG::SNET_GM_MOVE2GATE* pNetMsg = (GLMSG::SNET_GM_MOVE2GATE*) nmg;

	GLMSG::SNET_GM_MOVE2GATE_FLD NetMsgFld;
	NetMsgFld.dwGATE   = pNetMsg->dwGATE;
	NetMsgFld.dwGaeaID = dwGaeaID;
	
	SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}

// *****************************************************
// Desc: 특정맵의 특정위치에 이동 요청 처리
// *****************************************************
BOOL GLAgentServer::MsgGMMove2Pos ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pCHAR = GetChar ( dwGaeaID );
	if ( !pCHAR )						 return FALSE;
	if ( pCHAR->m_dwUserLvl < USER_GM3 ) return FALSE;

	GLMSG::SNET_GM_MOVE2MAPPOS* pNetMsg = (GLMSG::SNET_GM_MOVE2MAPPOS*) nmg;

	GLMSG::SNET_GM_MOVE2MAPPOS_FLD NetMsgFld;
	NetMsgFld.nidMAP   = pNetMsg->nidMAP;
	NetMsgFld.dwGaeaID = dwGaeaID;
	NetMsgFld.dwPOSX   = pNetMsg->dwPOSX;
	NetMsgFld.dwPOSY   = pNetMsg->dwPOSY;
	
	SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}

BOOL GLAgentServer::MsgServerGeneralChat ( NET_MSG_GENERIC* nmg )
{
	SENDTOALLCLIENT ( nmg );

	return TRUE;
}

BOOL GLAgentServer::ChatMsgLoudSpeakerAgt ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_CHAT_LOUDSPEAKER_AGT *pNetMsg = (GLMSG::SNETPC_CHAT_LOUDSPEAKER_AGT *) nmg;
	
	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )							return FALSE;
	if ( pChar->IsCHATBLOCK() )				return FALSE;

	NET_CHAT_FB NetMsgFB;
	NetMsgFB.emType = CHAT_TYPE_LOUDSPEAKER;
	NetMsgFB.sItemLink = pNetMsg->sItemLink; /*item link, Juver, 2017/07/31 */
	StringCchCopy ( NetMsgFB.szName, CHAR_SZNAME, pChar->m_szName );
	StringCchCopy ( NetMsgFB.szChatMsg, CHAT_MSG_SIZE, pNetMsg->szMSG );

	if ( pNetMsg->bchannel_all )	SENDTOALLCLIENT ( &NetMsgFB );
	else							SENDTOALLCLIENT ( &NetMsgFB, pChar->m_nChannel );

	return TRUE;
}

BOOL GLAgentServer::MsgClubNew2Agt ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_CLUB_NEW_2AGT *pNetMsg = (GLMSG::SNET_CLUB_NEW_2AGT *) nmg;

	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )		return FALSE;

	GLMSG::SNET_CLUB_NEW_FB NetMsgFB;
	NetMsgFB.emFB = EMCLUB_NEW_FB_FAIL;
	StringCchCopy ( NetMsgFB.szClubName, CHAR_SZNAME, pNetMsg->szClubName );

	if ( pChar->m_dwGuild!=CLUB_NULL )
	{
		NetMsgFB.emFB = EMCLUB_NEW_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	GLPARTY *pPARTY = m_cPartyMan.GetParty ( pChar->m_dwPartyID );
	if ( !pPARTY || !pPARTY->ISMASTER(pChar->m_dwGaeaID) )
	{
		NetMsgFB.emFB = EMCLUB_NEW_FB_NOTMASTER;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pPARTY->GETNUMBER() < GLCONST_CHAR::dwCLUB_PARTYNUM )
	{
		NetMsgFB.emFB = EMCLUB_NEW_FB_NOTMEMBER;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return S_FALSE;
	}

	GLPARTY::MEMBER_ITER pos = pPARTY->m_cMEMBER.begin();
	GLPARTY::MEMBER_ITER end = pPARTY->m_cMEMBER.end();
	for ( ; pos!=end; ++pos )
	{
		PGLCHARAG pMEMBER = GetChar ( (*pos) );
		if ( !pMEMBER )
		{
			NetMsgFB.emFB = EMCLUB_NEW_FB_NOTEXIST_MEM;
			SENDTOCLIENT ( dwClientID, &NetMsgFB );
			return FALSE;
		}

		if ( pMEMBER->m_dwGuild != CLUB_NULL )
		{
			NetMsgFB.emFB = EMCLUB_NEW_FB_MEM_ALREADY;
			SENDTOCLIENT ( dwClientID, &NetMsgFB );
			return FALSE;
		}
	}

	//	Note : 신청한 클럽 이름이 이미 사용중.
	//
	DWORD dwOLDGLUB = m_cClubMan.FindClub ( pNetMsg->szClubName );
	if ( dwOLDGLUB!=CLUB_NULL )
	{
		NetMsgFB.emFB = EMCLUB_NEW_FB_NAME;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Note : DB에 데이터 설정.
	//
	CCreateClub *pDbAction = new CCreateClub ( dwClientID, pNetMsg->szClubName, pChar->m_dwCharID );
	m_pDBMan->AddJob ( pDbAction );

	return TRUE;
}

BOOL GLAgentServer::MsgClubNewDb2Agt ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_CLUB_NEW_DB2AGT *pNetMsg = (GLMSG::SNET_CLUB_NEW_DB2AGT *) nmg;

	PGLCHARAG pChar = GetCharID ( pNetMsg->dwMasterID );
	if ( !pChar )
	{
		//	Note : db에 클럽을 만들었으나 클럽 마스터가 존제하지 않을 때는 클럽을 삭제한다.
		CDeleteClub *pDbAction = new CDeleteClub(pNetMsg->dwClubID,pNetMsg->dwMasterID);
		m_pDBMan->AddJob ( pDbAction );
		return FALSE;
	}

	GLMSG::SNET_CLUB_NEW_FB NetMsgFB;

	if ( pNetMsg->emFB==EMCLUB_NEW_FB_ALREADY )
	{
		NetMsgFB.emFB = EMCLUB_NEW_FB_ALREADY;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pNetMsg->emFB==EMCLUB_NEW_FB_NAME )
	{
		NetMsgFB.emFB = EMCLUB_NEW_FB_NAME;
		StringCchCopy ( NetMsgFB.szClubName, CHAR_SZNAME, pNetMsg->szClubName );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pNetMsg->emFB==EMCLUB_NEW_FB_FAIL )
	{
		NetMsgFB.emFB = EMCLUB_NEW_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	NetMsgFB.emFB = EMCLUB_NEW_FB_FAIL;
	StringCchCopy ( NetMsgFB.szClubName, CHAR_SZNAME, pNetMsg->szClubName );

	GLPARTY *pPARTY = m_cPartyMan.GetParty ( pChar->m_dwPartyID );
	if ( !pPARTY || !pPARTY->ISMASTER(pChar->m_dwGaeaID) )
	{
		NetMsgFB.emFB = EMCLUB_NEW_FB_NOTMASTER;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );

		CDeleteClub *pDbAction = new CDeleteClub(pNetMsg->dwClubID,pNetMsg->dwMasterID);
		m_pDBMan->AddJob ( pDbAction );

		return FALSE;
	}

	if ( pPARTY->GETNUMBER() < GLCONST_CHAR::dwCLUB_PARTYNUM )
	{
		NetMsgFB.emFB = EMCLUB_NEW_FB_NOTMEMBER;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );

		CDeleteClub *pDbAction = new CDeleteClub(pNetMsg->dwClubID,pNetMsg->dwMasterID);
		m_pDBMan->AddJob ( pDbAction );

		return S_FALSE;
	}

	//	Note : 클럽 설정.
	//
	GLCLUB cClubNew;
	cClubNew.m_dwID = pNetMsg->dwClubID;
	StringCchCopy(cClubNew.m_szName,CHAR_SZNAME,pNetMsg->szClubName);
	
	CTime cTIME_CUR = CTime::GetCurrentTime();
	cClubNew.m_tOrganize = cTIME_CUR.GetTime();

	cClubNew.m_dwMasterID = pChar->m_dwCharID;
	StringCchCopy(cClubNew.m_szMasterName,CHAR_SZNAME,pChar->m_szName);

	//	Note : 케릭터 개체에 클럽 ID 지정.
	//pChar->m_dwGuild = cClubNew.m_dwID;

	CTime cTIME_DIS(1970,2,1,1,1,1);
	cClubNew.m_tDissolution = cTIME_DIS.GetTime();

	//	Note : 멤버원 등록.
	{
		GLPARTY::MEMBER_ITER pos = pPARTY->m_cMEMBER.begin();
		GLPARTY::MEMBER_ITER end = pPARTY->m_cMEMBER.end();
		for ( ; pos!=end; ++pos )
		{
			PGLCHARAG pMEMBER = GetChar ( (*pos) );
			if ( !pMEMBER )								continue;
			if ( pMEMBER->m_dwGuild != CLUB_NULL )		continue;

			//	Note : 케릭터 개체에 클럽 ID 지정.
			pMEMBER->m_dwGuild = cClubNew.m_dwID;

			GLCLUBMEMBER cMEMBER;
			cMEMBER.dwID = pMEMBER->m_dwCharID;
			StringCchCopy ( cMEMBER.m_szName, CHAR_SZNAME, pMEMBER->m_szName );

			cClubNew.m_mapMembers.insert ( std::make_pair(cMEMBER.dwID,cMEMBER) );

			//	Note : DB에 멤버 설정.
			//
			CAddClubMember *pDbAction = new CAddClubMember ( cClubNew.m_dwID, cMEMBER.dwID );
			m_pDBMan->AddJob ( pDbAction );
		}
	}

	m_cClubMan.AddClub ( cClubNew );

	//	Note : 클럽 생성 성공.
	GLMSG::SNET_CLUB_NEW_2FLD NetMsgFLD;
	NetMsgFLD.emFB = EMCLUB_NEW_FB_OK;
	NetMsgFLD.dwCLUBID = cClubNew.m_dwID;
	StringCchCopy ( NetMsgFLD.szClubName, CHAR_SZNAME, cClubNew.m_szName );
	SENDTOFIELD ( dwClientID, &NetMsgFLD );

	//	Note : 생성 정보 전송.
	//
	GLMSG::SNET_CLUB_INFO_2FLD NetMsgInfo;
	NetMsgInfo.dwClubID = cClubNew.m_dwID;
	StringCchCopy ( NetMsgInfo.szClubName, CHAR_SZNAME, cClubNew.m_szName );

	NetMsgInfo.dwMasterID = cClubNew.m_dwMasterID;
	StringCchCopy ( NetMsgInfo.szMasterName, CHAR_SZNAME, cClubNew.m_szMasterName );
	NetMsgInfo.dwCDCertifior = cClubNew.m_dwCDCertifior;

	NetMsgInfo.dwRank = cClubNew.m_dwRank;
	NetMsgInfo.dwMarkVER = cClubNew.m_dwMarkVER;

	NetMsgInfo.tOrganize = cClubNew.m_tOrganize;
	NetMsgInfo.tDissolution = cClubNew.m_tDissolution;

	NetMsgInfo.dwMemberNum = cClubNew.GetNemberNum();
	{
		DWORD i=0;
		CLUBMEMBERS_ITER pos = cClubNew.m_mapMembers.begin();
		CLUBMEMBERS_ITER end = cClubNew.m_mapMembers.end();
		for ( ; pos!=end; ++pos )
		{
			const GLCLUBMEMBER &sCLUBMEMBER = (*pos).second;

			StringCchCopy ( NetMsgInfo.szMemberName[i], CHAR_SZNAME, sCLUBMEMBER.m_szName );
			NetMsgInfo.aMembers[i] = sCLUBMEMBER.dwID;
			++i;

			//	Note : 클럽 가입 로그 기록.
			GLITEMLMT::GetInstance().ReqAction
			(
				cClubNew.m_dwMasterID,		//	당사자.
				EMLOGACT_CLUB_JOIN_MEMBER,	//	행위.
				ID_CHAR, sCLUBMEMBER.dwID,	//	상대방.
				0,							//	exp
				0,							//	bright
				0,							//	life
				0							//	money
			);
		}
	}
	SENDTOALLCHANNEL ( &NetMsgInfo );

	return TRUE;
}

BOOL GLAgentServer::MsgClubDissolution ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_CLUB_DISSOLUTION *pNetMsg = (GLMSG::SNET_CLUB_DISSOLUTION *) nmg;

	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )		return FALSE;

	GLMSG::SNET_CLUB_DISSOLUTION_FB NetMsgFB;

	if ( pChar->m_dwGuild == CLUB_NULL )
	{
		NetMsgFB.emFB = EMCLUB_DIS_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	GLCLUB* pCLUB = m_cClubMan.GetClub ( pChar->m_dwGuild );
	if ( !pCLUB )
	{
		NetMsgFB.emFB = EMCLUB_DIS_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( !pCLUB->IsMaster( pChar->m_dwCharID ) )
	{
		NetMsgFB.emFB = EMCLUB_DIS_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Memo :	동맹에 가입되어 있다면 취소.
	if ( pCLUB->IsAlliance() )
	{
		NetMsgFB.emFB = EMCLUB_DIS_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pCLUB->GetAllBattleNum() > 0 )
	{
		NetMsgFB.emFB = EMCLUB_DIS_FB_CLUBBATTLE;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pNetMsg->bCANCEL )
	{
		if ( !pCLUB->IsRegDissolution() )
		{
			NetMsgFB.emFB = EMCLUB_DIS_FB_FAIL;
			SENDTOCLIENT ( dwClientID, &NetMsgFB );
			return FALSE;
		}

		pCLUB->DoDissolutionCancel ();

		//	Note : DB에 클럽 해산 시간 설정.
		CSetClubDissolutionTime *pDbAction = new CSetClubDissolutionTime(pCLUB->m_dwID,pCLUB->m_tDissolution);
		m_pDBMan->AddJob ( pDbAction );

		//	Note : 해체 취소 to FIELD
		//
		GLMSG::SNET_CLUB_DISSOLUTION_2FLD NetMsgFld;
		NetMsgFld.dwClubID = pCLUB->m_dwID;
		CTime tDisTime(1970,2,1,1,1,1);
		NetMsgFld.tDissolution = tDisTime.GetTime();
		SENDTOALLCHANNEL ( &NetMsgFld );

		//	Note : 해체 취소 FB
		//
		NetMsgFB.emFB = EMCLUB_DIS_FB_CANCEL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
	}
	else
	{
		//	Note : 해체 예정일 지정.
		CTime tDisTime = CTime::GetCurrentTime();
		CTimeSpan tADD(GLCONST_CHAR::dwCLUB_DISSOLUTION_DAY,0,10,0);
		tDisTime += tADD;

		pCLUB->DoDissolution(tDisTime.GetTime());

		//	Note : DB에 클럽 해산 시간 설정.
		CSetClubDissolutionTime *pDbAction = new CSetClubDissolutionTime(pCLUB->m_dwID,pCLUB->m_tDissolution);
		m_pDBMan->AddJob ( pDbAction );

		//	Note : 해체 FB.
		//
		NetMsgFB.emFB = EMCLUB_DIS_FB_OK;
		NetMsgFB.tDissolution = pCLUB->m_tDissolution;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );

		//	Note : 해체 to FIELD
		//
		GLMSG::SNET_CLUB_DISSOLUTION_2FLD NetMsgFld;
		NetMsgFld.dwClubID = pCLUB->m_dwID;
		NetMsgFld.tDissolution = pCLUB->m_tDissolution;
		SENDTOALLCHANNEL ( &NetMsgFld );
	}

	return TRUE;
}

BOOL GLAgentServer::MsgClubCertified ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_CLUB_CERTIFIED_AGT *pNetMsg = (GLMSG::SNET_CLUB_CERTIFIED_AGT *) nmg;

	bool bOK = GLGuidanceAgentMan::GetInstance().ChangeGuidClub ( pNetMsg->dwID, pNetMsg->dwCLUBID );
	if ( !bOK )		return FALSE;

	//	Note : 필드서버에 알림.
	//
	GLMSG::SNET_CLUB_CERTIFIED_FLD NetMsgFld;
	NetMsgFld.dwID = pNetMsg->dwID;
	NetMsgFld.dwCLUBID = pNetMsg->dwCLUBID;
	SENDTOALLCHANNEL ( &NetMsgFld );

	//	Note : 클라이언트틀에게 알림.
	//
	std::string strGuidName, strClubName;
	GLGuidance *pGuid = GLGuidanceAgentMan::GetInstance().Find ( pNetMsg->dwID );
	if ( pGuid )	strGuidName = pGuid->m_strName;

	GLCLUB *pCLUB = m_cClubMan.GetClub ( pNetMsg->dwCLUBID );
	if ( pCLUB )	strClubName = pCLUB->m_szName;

	//	Note : 클라이언트에 알림.
	GLMSG::SNET_EMGUIDCLUB_CERTIFIED NetMsgCertified;
	StringCchCopy ( NetMsgCertified.szMapName, CHAR_SZNAME, strGuidName.c_str() );	
	StringCchCopy ( NetMsgCertified.szGuildName, CHAR_SZNAME, strClubName.c_str() );	
	SENDTOALLCLIENT ( &NetMsgCertified );

	/*CString strTEXT;
	strTEXT.Format ( ID2SERVERTEXT("EMGUIDCLUB_CERTIFIED"), strClubName.c_str(), strGuidName.c_str() );

	GLMSG::SNET_SERVER_GENERALCHAT NetMsg;
	NetMsg.SETTEXT ( strTEXT.GetString() );
	SENDTOALLCLIENT ( &NetMsg );*/

	return TRUE;
}

BOOL GLAgentServer::MsgClubIncomeMoney ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNET_CLUB_INCOME_MONEY_AGT *pNetMsg = (GLMSG::SNET_CLUB_INCOME_MONEY_AGT *) nmg;

	GLCLUB *pCLUB = m_cClubMan.GetClub ( pNetMsg->dwID );
	if ( !pCLUB )		return FALSE;

	if ( pNetMsg->lnMoney < 0 ) return FALSE;

	//	Note : 클럽의 수익을 반영.
	pCLUB->m_lnIncomeMoney += LONGLONG ( pNetMsg->lnMoney * GLCONST_CHAR::fEARNING_RATE + 0.1f );

	GLITEMLMT::GetInstance().ReqMoneyExc(	ID_CLUB, pNetMsg->dwID, 
											ID_CLUB, pNetMsg->dwID, 
											pNetMsg->lnMoney, 
											EMITEM_ROUTE_CLUBINCOME );

	//CDebugSet::ToFile ( "club_income_agent.txt", "{MsgClubIncomeMoney}, ClubID[%u], MoneyFromField[%I64d], IncomeMoney[%I64d]",
	//					pNetMsg->dwID, pNetMsg->lnMoney, pCLUB->m_lnIncomeMoney );

	//	Note : Db에 저장.
	CSetClubIncomeMoney *pDbAction = new CSetClubIncomeMoney ( pCLUB->m_dwID, pCLUB->m_lnIncomeMoney );
	m_pDBMan->AddJob ( pDbAction );

	return TRUE;
}

BOOL GLAgentServer::MsgClubIncomeMoneyUp ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNET_CLUB_INCOME_UP *pNetMsg = (GLMSG::SNET_CLUB_INCOME_UP *) nmg;

	GLCLUB *pCLUB = m_cClubMan.GetClub ( pNetMsg->dwClubID );
	if ( !pCLUB )		return FALSE;

	//	Note : 클럽의 수익을 반영.
	if ( pCLUB->m_lnIncomeMoney > 0 )
	{
		GLMSG::SNET_CLUB_INCOME_DN NetMsgDn;
		NetMsgDn.dwClubID = pCLUB->m_dwID;
		NetMsgDn.lnInComeMoney = pCLUB->m_lnIncomeMoney;
		SENDTOALLCHANNEL ( &NetMsgDn );

		pCLUB->m_lnIncomeMoney = 0;

		//CDebugSet::ToFile ( "club_income_agent.txt", "{MsgClubIncomeMoneyUp}, ClubID[%u], IncomeMoney[%I64d]",
		//					pCLUB->m_dwID, NetMsgDn.lnInComeMoney );

		//	Note : Db에 저장.
		CSetClubIncomeMoney *pDbAction = new CSetClubIncomeMoney ( pCLUB->m_dwID, pCLUB->m_lnIncomeMoney );
		m_pDBMan->AddJob ( pDbAction );
	}

	return TRUE;
}

// *****************************************************
// Desc: 클럽수익에서 돈을 빼간다 incomemoney->storagemoney
// *****************************************************
//BOOL GLAgentServer::MsgClubIncomeMoneyFb ( NET_MSG_GENERIC* nmg )
//{
//	GLMSG::SNET_CLUB_INCOME_FB *pNetMsg = (GLMSG::SNET_CLUB_INCOME_FB *) nmg;
//
//	GLCLUB *pCLUB = m_cClubMan.GetClub ( pNetMsg->dwClubID );
//	if ( !pCLUB )		return FALSE;
//
//	if( pNetMsg->lnInComeMoney <= 0 ) return FALSE;
//
//	// 돈을 더 많이 빼가려구 시도해서 DB에서 Overflow 생김
//	if ( pCLUB->m_lnIncomeMoney < pNetMsg->lnInComeMoney ) return FALSE;
//
//	//	Note : 클럽의 수익이 반영된뒤 정산.
//	pCLUB->m_lnIncomeMoney -= pNetMsg->lnInComeMoney;
//
//	//m_pConsoleMsg->Write( LOG_CONSOLE, "m_lnIncomeMoney : %d", pCLUB->m_lnIncomeMoney );
//
//	//	Note : Db에 저장.
//	CSetClubIncomeMoney *pDbAction = new CSetClubIncomeMoney ( pCLUB->m_dwID, pCLUB->m_lnIncomeMoney );
//	m_pDBMan->AddJob ( pDbAction );
//
//	return TRUE;
//}

BOOL GLAgentServer::MsgClubMemberDb2Del ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_CLUB_MEMBER_DB2DEL *pNetMsg = (GLMSG::SNET_CLUB_MEMBER_DB2DEL *) nmg;

	if ( pNetMsg->dwClub==CLUB_NULL )
	{
		return S_FALSE;
	}

	GLClubMan &sClubMan = GLAgentServer::GetInstance().GetClubMan();
	GLCLUB *pCLUB = sClubMan.GetClub ( pNetMsg->dwClub );
	if ( !pCLUB )
	{
		return S_FALSE;
	}

	if ( pCLUB->m_dwMasterID==pNetMsg->dwMember )
	{
		return S_FALSE;
	}

	bool bFOUND = pCLUB->IsMember ( pNetMsg->dwMember );
	if ( !bFOUND )
	{
		return S_FALSE;
	}

	//	Note : 클럽 맴버 탈퇴 로그 기록.
	GLITEMLMT::GetInstance().ReqAction
	(
		pCLUB->m_dwMasterID,		//	당사자.
		EMLOGACT_CLUB_SECEDE_MEMBER,//	행위.
		ID_CHAR, pNetMsg->dwMember,	//	상대방.
		0,							//	exp
		0,							//	bright
		0,							//	life
		0							//	money
	);

	//	Note : DB에서 삭제.
	CDeleteClubMember *pDbAction = new CDeleteClubMember(pNetMsg->dwMember);
	GLAgentServer::GetInstance().GetDBMan()->AddJob ( pDbAction );

	//	Note : 종전 인증 권한자 백업.
	DWORD dwCD_OLD = pCLUB->m_dwCDCertifior;

	//	CDM 권한 있을경우 제거
	pCLUB->SetMemberCDMFlag( pNetMsg->dwMember, FALSE );

	//	Note : 클럽 목록에서 제거.
	pCLUB->DelMember ( pNetMsg->dwMember );

	//	Note : 선도클럽 인증자 변경시.
	if ( dwCD_OLD!=pCLUB->m_dwCDCertifior )
	{
		//	Note : 서버에 인증 권한자 저장.
		CDbAction *pDbAction = new CSetClubDeputy ( pCLUB->m_dwID, pCLUB->m_dwCDCertifior );
		GLDBMan* pDbMan = GLAgentServer::GetInstance().GetDBMan();
		if ( pDbMan )	pDbMan->AddJob ( pDbAction );

		//	Note : 자기 클럽원에게 인증권한 알림.
		GLMSG::SNET_CLUB_SUBMASTER_BRD NetMsgBrd;
		NetMsgBrd.dwCharID = pCLUB->m_dwMasterID;	//  ( 마스터 ID 를 사용. )
		NetMsgBrd.dwFlags = NULL;
		NetMsgBrd.dwCDCertifior = pCLUB->m_dwCDCertifior;
		GLAgentServer::GetInstance().SENDTOCLUBCLIENT ( pCLUB->m_dwID, &NetMsgBrd );
	}

	//	Note : 맴버에서 삭제 필드에 알림.
	GLMSG::SNET_CLUB_MEMBER_DEL_2FLD NetMsgFld;
	NetMsgFld.dwClub = pNetMsg->dwClub;
	NetMsgFld.dwMember = pNetMsg->dwMember;
	SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}



BOOL GLAgentServer::MsgGmViewWorkEvent ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_VIEWWORKEVENT_FB NetMsgFB;

	if( m_WorkEvent.bWorkEvent[0] )  // 경험치 
	{
		sprintf( NetMsgFB.szWorkEventInfo, "Event Type : Exp, Event Rate : %.2f", m_WorkEvent.exp_Event.fRATE );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
	}

	if( m_WorkEvent.bWorkEvent[1] )  // 아이템
	{
		sprintf( NetMsgFB.szWorkEventInfo, "Event Type : Item, Event Rate : %.2f", m_WorkEvent.item_Event.fRATE );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
	}

	if( m_WorkEvent.bWorkEvent[2] )  // 돈 드랍율
	{
		sprintf( NetMsgFB.szWorkEventInfo, "Event Type : Money, Event Rate : %.2f", m_WorkEvent.money_Event.fRATE );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
	}

	if( m_WorkEvent.bWorkEvent[3] )  // EX 이벤트
	{
		char szTempChar[32] = {0,};
		if( m_WorkEvent.ex_Event.emType & EMGM_EVENT_SPEED )
			strcat( szTempChar, "SPEED,");
		if( m_WorkEvent.ex_Event.emType & EMGM_EVENT_ASPEED )
			strcat( szTempChar, "ATTACK SPEED,");
		if( m_WorkEvent.ex_Event.emType & EMGM_EVENT_ATTACK )
			strcat( szTempChar, "ATTACK RATE,");
		
		szTempChar[strlen(szTempChar)-1] = '\0';
		sprintf( NetMsgFB.szWorkEventInfo, "Event Type : Ex Event[%s] Event Rate : %d", szTempChar, m_WorkEvent.ex_Event.wValue );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
	}

	if( m_WorkEvent.bWorkEvent[4] )  // 리미트 이벤트 
	{
		sprintf( NetMsgFB.szWorkEventInfo, "Event Type : LimitEvent, Start_LV %d, End_LV %d, Play_Time %d, Boost_Time %d, Item Rate : %.2f Exp Rate : %.2f, EventMinute %d", 
				m_WorkEvent.limit_Event.start_Lv, m_WorkEvent.limit_Event.end_Lv, m_WorkEvent.limit_Event.play_Time, m_WorkEvent.limit_Event.buster_Time, 
				m_WorkEvent.limit_Event.expGain_Rate, m_WorkEvent.limit_Event.itemGain_Rate, m_WorkEvent.limit_Event.dwEventMinute );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
	}
	

	return TRUE;
}

BOOL GLAgentServer::MsgGmEventEx ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_EVENT_EX *pNetMsg = (GLMSG::SNET_GM_EVENT_EX *)nmg;
	if( pNetMsg->wValue < 1 )		pNetMsg->wValue = 1;
	else if( pNetMsg->wValue > 65535 )	pNetMsg->wValue = 65535;

	SENDTOALLCHANNEL ( pNetMsg );

	m_WorkEvent.bWorkEvent[3] = TRUE;
	m_WorkEvent.ex_Event	  = *pNetMsg;

	return TRUE;
}

BOOL GLAgentServer::MsgGmEventExEnd ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	SENDTOALLCHANNEL ( nmg );

	m_WorkEvent.bWorkEvent[3] = FALSE;

	return TRUE;
}

// 이벤트 시작!!
BOOL GLAgentServer::MsgGmLimitEventBegin ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	//** Add EventTime
	GLMSG::SNET_GM_LIMIT_EVENT_BEGIN *pNetMsg = (GLMSG::SNET_GM_LIMIT_EVENT_BEGIN *)nmg;

	m_sEventState.bEventStart = TRUE;
	m_sEventState.EventBusterTime  = pNetMsg->buster_Time * 60;
	m_sEventState.EventPlayTime   = pNetMsg->play_Time * 60;

	m_sEventState.fExpGainRate     = pNetMsg->expGain_Rate;
	m_sEventState.fItemGainRate    = pNetMsg->itemGain_Rate;
	m_sEventState.MinEventLevel	   = pNetMsg->start_Lv;
	m_sEventState.MaxEventLevel    = pNetMsg->end_Lv;
	m_sEventState.dwEventEndMinute = pNetMsg->dwEventMinute;
	
	m_sEventState.EventStartTime  = CTime::GetCurrentTime();

	CTime	  crtTime      = CTime::GetCurrentTime();
	for ( DWORD i=0; i<m_dwMaxClient; ++i )
	{
		PGLCHARAG pChar = m_PCArray[i];
		if ( pChar )
		{
			pChar->m_sEventTime.loginTime = crtTime.GetTime();
		}
	}

	SENDTOALLCHANNEL ( pNetMsg );

	m_WorkEvent.bWorkEvent[4] = TRUE;
	m_WorkEvent.limit_Event	  = *pNetMsg;

	return TRUE;
}

// 이벤트 종료!!
BOOL GLAgentServer::MsgGmLimitEventEnd ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	m_sEventState.bEventStart = false;

	GLMSG::SNET_GM_LIMIT_EVENT_END *pNetMsg = (GLMSG::SNET_GM_LIMIT_EVENT_END *)nmg;

	SENDTOALLCHANNEL ( nmg );

	m_WorkEvent.bWorkEvent[4] = FALSE;

	return TRUE;
}


BOOL GLAgentServer::MsgGmLimitEventRestart ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;

	GLMSG::SNET_GM_LIMIT_EVENT_RESTART *pNetMsg = (GLMSG::SNET_GM_LIMIT_EVENT_RESTART *)nmg;
	
	pMyChar->m_sEventTime.loginTime = pNetMsg->restartTime;


	return TRUE;
}

BOOL GLAgentServer::MsgGmLimitEventTimeReq( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
//	GLMSG::SNET_GM_LIMIT_EVENT_TIME_REQ *pNetMsg = (GLMSG::SNET_GM_LIMIT_EVENT_TIME_REQ *)nmg;;

	PGLCHARAG pCHAR = GetChar ( dwGaeaID );
	if ( !pCHAR )							return FALSE;

	GLMSG::SNET_GM_LIMIT_EVENT_TIME_REQ_FB NetSendMsg;

	if( m_sEventState.bEventStart == FALSE )
	{
		NetSendMsg.bEventStart = FALSE;
	}else{
		NetSendMsg.bEventStart = TRUE;

		NetSendMsg.buster_Time = m_sEventState.EventBusterTime;
		NetSendMsg.play_Time   = m_sEventState.EventPlayTime;
		NetSendMsg.start_Lv	   = m_sEventState.MinEventLevel;
		NetSendMsg.end_Lv	   = m_sEventState.MaxEventLevel;

		// 만약 처음 게임을 시작한다면 로그인 시간을 RESET한다.
		if( pCHAR->m_bReciveEventTime == FALSE )
		{
			CTime	  crtTime      = CTime::GetCurrentTime();

			pCHAR->m_sEventTime.loginTime = crtTime.GetTime();
			NetSendMsg.loginTime   = pCHAR->m_sEventTime.loginTime;

			GLMSG::SNET_GM_LIMIT_EVENT_TIME_RESET NetResetMsg;
			NetResetMsg.dwGaeaID  = pCHAR->m_dwGaeaID;
			NetResetMsg.loginTime = crtTime.GetTime();
			SENDTOFIELD ( pCHAR->m_dwClientID, &NetResetMsg );


			pCHAR->m_bReciveEventTime = TRUE;

		}else{
			NetSendMsg.loginTime   = pCHAR->m_sEventTime.loginTime;
		}
	}
        




	SENDTOCLIENT ( pCHAR->m_dwClientID, &NetSendMsg );

	return TRUE;
}


BOOL GLAgentServer::MsgCyberCafeClassUpdate( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pCHAR = GetChar ( dwGaeaID );
	if ( !pCHAR )							return FALSE;

	GLMSG::SNET_CYBERCAFECLASS_UPDATE *pNetMsg = (GLMSG::SNET_CYBERCAFECLASS_UPDATE *)nmg;
	pCHAR->m_dwThaiCCafeClass = pNetMsg->dwCyberCafeClass;
	pCHAR->m_nMyCCafeClass = pNetMsg->dwCyberCafeClass;

	SENDTOFIELD ( pCHAR->m_dwClientID, pNetMsg );
	SENDTOCLIENT ( pCHAR->m_dwClientID, pNetMsg );

	return TRUE;
}

BOOL GLAgentServer::MsgGmEventExp( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;
	GLMSG::SNET_GM_EVENT_EXP *pNetMsg = (GLMSG::SNET_GM_EVENT_EXP *)nmg;
	if( pNetMsg->fRATE < 1.0f ) pNetMsg->fRATE = 1.0f;
	if( pNetMsg->fRATE > 10.0f ) pNetMsg->fRATE = 10.0f;

	if ( pNetMsg->dwCHARID != pMyChar->m_dwCharID )	return FALSE;

	GLMSG::SNET_GM_EVENT_EXP NetMsgField;
	NetMsgField.fRATE = pNetMsg->fRATE;
	NetMsgField.dwCHARID = pMyChar->m_dwCharID;
	SENDTOALLCHANNEL ( &NetMsgField );

	m_WorkEvent.bWorkEvent[0] = TRUE;
	m_WorkEvent.exp_Event	  = *pNetMsg;

	return TRUE;
}

BOOL GLAgentServer::MsgGmEventExpEnd( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_EVENT_EXP_END *pNetMsg = (GLMSG::SNET_GM_EVENT_EXP_END *)nmg;
	if ( pNetMsg->dwCHARID != pMyChar->m_dwCharID )	return FALSE;

	GLMSG::SNET_GM_EVENT_EXP_END NetMsgField;
	NetMsgField.dwCHARID = pMyChar->m_dwCharID;
	SENDTOALLCHANNEL ( &NetMsgField );

	m_WorkEvent.bWorkEvent[0] = FALSE;

	return TRUE;
}

BOOL GLAgentServer::MsgGmEventItemGen ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_EVENT_ITEM_GEN *pNetMsg = (GLMSG::SNET_GM_EVENT_ITEM_GEN *)nmg;
	if( pNetMsg->fRATE < 1.0f )		pNetMsg->fRATE = 1.0f;
	if( pNetMsg->fRATE > 10.0f )	pNetMsg->fRATE = 10.0f;

	if ( pNetMsg->dwCHARID != pMyChar->m_dwCharID )	return FALSE;

	GLMSG::SNET_GM_EVENT_ITEM_GEN NetMsgField;
	NetMsgField.fRATE = pNetMsg->fRATE;
	NetMsgField.dwCHARID = pMyChar->m_dwCharID;
	SENDTOALLCHANNEL ( &NetMsgField );

	m_WorkEvent.bWorkEvent[1] = TRUE;
	m_WorkEvent.item_Event	  = *pNetMsg;

	return TRUE;
}

BOOL GLAgentServer::MsgGmEventItemGenEnd ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_EVENT_ITEM_GEN_END *pNetMsg = (GLMSG::SNET_GM_EVENT_ITEM_GEN_END *)nmg;
	if ( pNetMsg->dwCHARID != pMyChar->m_dwCharID )	return FALSE;

	GLMSG::SNET_GM_EVENT_ITEM_GEN_END NetMsgField;
	NetMsgField.dwCHARID = pMyChar->m_dwCharID;
	SENDTOALLCHANNEL ( &NetMsgField );

	m_WorkEvent.bWorkEvent[1] = FALSE;

	return TRUE;
}

BOOL GLAgentServer::MsgGmEventMoneyGen ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_EVENT_MONEY_GEN *pNetMsg = (GLMSG::SNET_GM_EVENT_MONEY_GEN *)nmg;
	if( pNetMsg->fRATE < 1.0f )		pNetMsg->fRATE = 1.0f;
	if( pNetMsg->fRATE > 10.0f )	pNetMsg->fRATE = 10.0f;

	if ( pNetMsg->dwCHARID != pMyChar->m_dwCharID )	return FALSE;

	GLMSG::SNET_GM_EVENT_MONEY_GEN NetMsgField;
	NetMsgField.fRATE = pNetMsg->fRATE;
	NetMsgField.dwCHARID = pMyChar->m_dwCharID;
	SENDTOALLCHANNEL ( &NetMsgField );

	m_WorkEvent.bWorkEvent[2] = TRUE;
	m_WorkEvent.money_Event	  = *pNetMsg;

	return TRUE;
}

BOOL GLAgentServer::MsgGmEventMoneyGenEnd ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_EVENT_MONEY_GEN_END *pNetMsg = (GLMSG::SNET_GM_EVENT_MONEY_GEN_END *)nmg;
	if ( pNetMsg->dwCHARID != pMyChar->m_dwCharID )	return FALSE;

	GLMSG::SNET_GM_EVENT_MONEY_GEN_END NetMsgField;
	NetMsgField.dwCHARID = pMyChar->m_dwCharID;
	SENDTOALLCHANNEL ( &NetMsgField );

	m_WorkEvent.bWorkEvent[2] = FALSE;

	return TRUE;
}

BOOL GLAgentServer::MsgGmBigHead ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM2 )	return FALSE;

	GLMSG::SNET_GM_BIGHEAD* pNetMsg = (GLMSG::SNET_GM_BIGHEAD*) nmg;

	//	Note : 모든 필드 서버에 송신.
	//
	SENDTOALLCHANNEL ( nmg );

	//	Note : 모든 클라이언트에 송신.
	//
	GLMSG::SNET_GM_BIGHEAD_BRD NetMsgBrd;
	NetMsgBrd.bBIGHEAD = pNetMsg->bBIGHEAD;
	SENDTOALLCLIENT ( &NetMsgBrd );

	return TRUE;
}

BOOL GLAgentServer::MsgGmBigHand ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM2 )	return FALSE;

	GLMSG::SNET_GM_BIGHAND* pNetMsg = (GLMSG::SNET_GM_BIGHAND*) nmg;

	//	Note : 모든 필드 서버에 송신.
	//
	SENDTOALLCHANNEL ( nmg );

	//	Note : 모든 클라이언트에 송신.
	//
	GLMSG::SNET_GM_BIGHAND_BRD NetMsgBrd;
	NetMsgBrd.bBIGHAND = pNetMsg->bBIGHAND;
	SENDTOALLCLIENT ( &NetMsgBrd );

	return TRUE;
}

BOOL GLAgentServer::MsgGmFreePK ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM2 )	return FALSE;

	GLMSG::SNET_GM_FREEPK* pNetMsg = ( GLMSG::SNET_GM_FREEPK*) nmg;

	m_cFreePK.StartFREEPKMODE ( pNetMsg->dwPKTIME );

	SENDTOALLCHANNEL ( nmg );
	
	GLMSG::SNET_GM_FREEPK_BRD NetMsgBrd;
	NetMsgBrd.dwPKTIME = pNetMsg->dwPKTIME;
	SENDTOALLCLIENT ( &NetMsgBrd );

	return TRUE;
}

/*private market set, Juver, 2018/01/02 */
BOOL GLAgentServer::MsgGmSetPrivateMarket ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_SET_PRIVATE_MARKET* pNetMsg = ( GLMSG::SNET_GM_SET_PRIVATE_MARKET*) nmg;

	m_pConsoleMsg->Write ( LOG_TEXT_CONSOLE, "Set Private Market :%s", pNetMsg->bEnable? "Enable":"Disable" );

	SENDTOALLCHANNEL ( nmg );

	GLMSG::SNET_GM_SET_PRIVATE_MARKET_BRD NetMsgBrd;
	NetMsgBrd.bEnable = pNetMsg->bEnable;
	SENDTOALLCLIENT ( &NetMsgBrd );

	return TRUE;
}

/*megaphone set, Juver, 2018/01/02 */
BOOL GLAgentServer::MsgGmSetMegaPhone ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_SET_MEGAPHONE* pNetMsg = ( GLMSG::SNET_GM_SET_MEGAPHONE*) nmg;

	m_pConsoleMsg->Write ( LOG_TEXT_CONSOLE, "Set Megaphone :%s", pNetMsg->bEnable? "Enable":"Disable" );

	SENDTOALLCHANNEL ( nmg );

	GLMSG::SNET_GM_SET_MEGAPHONE_BRD NetMsgBrd;
	NetMsgBrd.bEnable = pNetMsg->bEnable;
	SENDTOALLCLIENT ( &NetMsgBrd );

	return TRUE;
}


BOOL GLAgentServer::MsgGmViewAllPlayer ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_VIEWALLPLAYER_FLD_REQ NetMsgFld;
	NetMsgFld.dwID = dwGaeaID;

	// 필드서버에 요청
	SENDTOFIELD ( pMyChar->m_dwClientID, &NetMsgFld );

	return TRUE;
}

// *****************************************************
// Desc: 지정된 케릭터를 자기 옆으로 소환
// *****************************************************
BOOL GLAgentServer::MsgGmGenChar ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNETPC_GM_GENCHAR* pNetMsg = (GLMSG::SNETPC_GM_GENCHAR*) nmg;
	GLMSG::SNETPC_GM_GENCHAR_FB NetMsgFB;

	PGLCHARAG pGenCHAR;

	if ( pNetMsg->dwCHARID != 0 )
	{
		pGenCHAR = GetCharID ( pNetMsg->dwCHARID );
	}
	else
	{
		pGenCHAR = GetChar ( pNetMsg->szNAME );
	}

	if ( !pGenCHAR )
	{
		NetMsgFB.emFB = EMGM_MOVE2CHAR_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pGenCHAR->m_dwCurFieldSvr==FIELDSERVER_MAX )
	{
		NetMsgFB.emFB = EMGM_MOVE2CHAR_FB_TO_CONDITION;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pGenCHAR->m_nChannel!=pMyChar->m_nChannel )
	{
		NetMsgFB.emFB = EMGM_MOVE2CHAR_FB_CHANNEL;
		NetMsgFB.nChannel = pGenCHAR->m_nChannel;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	// GM의 위치를 확인한다.
	GLMSG::SNETPC_GM_MOVE2CHAR_POS NetMsgCK;
	NetMsgCK.dwCOMMAND_CHARID = pGenCHAR->m_dwCharID;
	NetMsgCK.dwTO_CHARID = pMyChar->m_dwCharID;
	SENDTOFIELDSVR ( pMyChar->m_nChannel, pMyChar->m_dwCurFieldSvr, &NetMsgCK );

	NetMsgFB.emFB       = EMGM_MOVE2CHAR_FB_OK;
	
	SENDTOCLIENT ( dwClientID, &NetMsgFB );

	return TRUE;
}


BOOL GLAgentServer::MsgGmPrintCrowList ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pCHAR = GetChar ( dwGaeaID );
	if ( !pCHAR )							return FALSE;
	if ( pCHAR->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_PRINT_CROWLIST* pNetMsg = (GLMSG::SNET_GM_PRINT_CROWLIST*) nmg;

	GLMSG::SNET_GM_PRINT_CROWLIST_FLD NetMsgFld;
	NetMsgFld.dwFindMob = pNetMsg->dwFindMob;
	NetMsgFld.dwGaeaID  = dwGaeaID;
	SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}

BOOL GLAgentServer::MsgGmGenMob ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_MOB_GEN* pNetMsg = (GLMSG::SNET_GM_MOB_GEN*) nmg;

	GLMSG::SNET_GM_MOB_GEN_FLD NetMsgFld;

	NetMsgFld.dwGaeaID = dwGaeaID;
	NetMsgFld.wMainID = pNetMsg->wMainID;
	NetMsgFld.wSubID = pNetMsg->wSubID;
	NetMsgFld.wPosX = pNetMsg->wPosX;
	NetMsgFld.wPosY = pNetMsg->wPosY;
	
	SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}

BOOL GLAgentServer::MsgGmGenMobEx ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_MOB_GEN_EX* pNetMsg = (GLMSG::SNET_GM_MOB_GEN_EX*) nmg;

	SNATIVEID sMapID(pNetMsg->wMapMID,pNetMsg->wMapSID);
	GLMapList::FIELDMAP MapsList = m_sMapList.GetMapList ();
	GLMapList::FIELDMAP_ITER iter = MapsList.find ( sMapID.dwID );
	if ( iter==MapsList.end() ) return FALSE;	

	GLMSG::SNET_GM_MOB_GEN_EX_FLD NetMsgFld;
	NetMsgFld.wMobMID  = pNetMsg->wMobMID;
	NetMsgFld.wMobSID  = pNetMsg->wMobSID;
	NetMsgFld.wMapMID  = pNetMsg->wMapMID;
	NetMsgFld.wMapSID  = pNetMsg->wMapSID;
	NetMsgFld.wPosX	   = pNetMsg->wPosX;
	NetMsgFld.wPosY    = pNetMsg->wPosY;
	NetMsgFld.wRange   = pNetMsg->wRange;
	NetMsgFld.wNum     = pNetMsg->wNum;
	NetMsgFld.dwGaeaID = dwGaeaID;
	
	if ( pNetMsg->bThisChannel ) SENDTOCHANNEL ( &NetMsgFld, pMyChar->m_nChannel );
	else						 SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}

BOOL GLAgentServer::MsgGmDelMobEx ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_MOB_DEL_EX* pNetMsg = (GLMSG::SNET_GM_MOB_DEL_EX*) nmg;

	SNATIVEID sMapID(pNetMsg->wMapMID,pNetMsg->wMapSID);
	GLMapList::FIELDMAP MapsList = m_sMapList.GetMapList ();
	GLMapList::FIELDMAP_ITER iter = MapsList.find ( sMapID.dwID );
	if ( iter==MapsList.end() ) return FALSE;	

	GLMSG::SNET_GM_MOB_DEL_EX_FLD NetMsgFld;
	NetMsgFld.wMobMID  = pNetMsg->wMobMID;
	NetMsgFld.wMobSID  = pNetMsg->wMobSID;
	NetMsgFld.wMapMID  = pNetMsg->wMapMID;
	NetMsgFld.wMapSID  = pNetMsg->wMapSID;
	
	if ( pNetMsg->bThisChannel ) SENDTOCHANNEL ( &NetMsgFld, pMyChar->m_nChannel );
	else						 SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}

BOOL GLAgentServer::MsgGmDelMob( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_MOB_DEL* pNetMsg = (GLMSG::SNET_GM_MOB_DEL*) nmg;

	GLMSG::SNET_GM_MOB_DEL_FLD NetMsgFld;
	NetMsgFld.dwGaeaID	= dwGaeaID;
	NetMsgFld.sMobID	= pNetMsg->sMobID;
	NetMsgFld.bMaterial	= pNetMsg->bMaterial;

	SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}

BOOL GLAgentServer::MsgGmMobLevel ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pCHAR = GetChar ( dwGaeaID );
	if ( !pCHAR )							return FALSE;
	if ( pCHAR->m_dwUserLvl < USER_GM1 )	return FALSE;

	SENDTOALLCHANNEL ( nmg );

	return TRUE;
}

BOOL GLAgentServer::MsgGmMobLevelClear ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pCHAR = GetChar ( dwGaeaID );
	if ( !pCHAR )							return FALSE;
	if ( pCHAR->m_dwUserLvl < USER_GM1 )	return FALSE;

	SENDTOALLCHANNEL ( nmg );

	return TRUE;
}

BOOL GLAgentServer::MsgGmWhereNPC ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pCHAR = GetChar ( dwGaeaID );
	if ( !pCHAR )							return FALSE;
	if ( pCHAR->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_WHERE_NPC* pNetMsg = (GLMSG::SNET_GM_WHERE_NPC*) nmg;

	GLMSG::SNET_GM_WHERE_NPC_FLD NetMsgFld;

	NetMsgFld.nidNPC   = pNetMsg->nidNPC;
	NetMsgFld.dwGaeaID = dwGaeaID;
	SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}


// *****************************************************
// Desc: 친구위치 확인후 필드서버에 이동 요청 (field->Agent)
// *****************************************************
BOOL GLAgentServer::Msg2FriendAG ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNETPC_2_FRIEND_AG *pNetMsg = (GLMSG::SNETPC_2_FRIEND_AG *) nmg;
	GLMSG::SNETPC_2_FRIEND_FB NetMsgFB;

	PGLCHARAG pCHAR = GetCharID ( pNetMsg->dwCOMMAND_CHARID );
	if ( !pCHAR )	return FALSE;

	if ( pNetMsg->emFB==EM2FRIEND_FB_OK )
	{
		GLMSG::SNETPC_2_FRIEND_FLD NetMsgFld;
		NetMsgFld.sFriendMapID = pNetMsg->sFriendMapID;
		NetMsgFld.vFriendPos = pNetMsg->vFriendPos;
		NetMsgFld.wItemPosX = pNetMsg->wItemPosX;
		NetMsgFld.wItemPosY = pNetMsg->wItemPosY;
		SENDTOFIELD ( pCHAR->m_dwClientID, &NetMsgFld );
	}
	else
	{
		//	Note : 친구에게 이동 실패 반환.
		NetMsgFB.emFB = pNetMsg->emFB;
		SENDTOCLIENT ( pCHAR->m_dwClientID, &NetMsgFB );
	}

	return TRUE;
}

// *****************************************************
// Desc: 친구이동 요청 처리
// *****************************************************
BOOL GLAgentServer::Msg2FriendReq ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_2_FRIEND_REQ* pNetMsg = (GLMSG::SNETPC_2_FRIEND_REQ*) nmg;
	GLMSG::SNETPC_2_FRIEND_FB NetMsgFB;

	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;

	GLCHARAG_DATA::MAPFRIEND_ITER pos = pMyChar->m_mapFriend.find ( pNetMsg->szFRIEND_NAME );
	if ( pos==pMyChar->m_mapFriend.end() )
	{
		NetMsgFB.emFB = EM2FRIEND_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

//	SFRIEND * pFRIEND = (*pos).second;
//	if( !pFRIEND ) return FALSE;
	SFRIEND &sFRIEND = (*pos).second;


	if ( !sFRIEND.bONLINE )
//	if ( !pFRIEND->bONLINE )
	{
		NetMsgFB.emFB = EM2FRIEND_FB_FRIEND_CONDITION;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

//	PGLCHARAG pCHAR_FRIEND = GetCharID ( pFRIEND->nCharID );
	PGLCHARAG pCHAR_FRIEND = GetCharID ( sFRIEND.nCharID );
	if ( !pCHAR_FRIEND )
	{
		NetMsgFB.emFB = EM2FRIEND_FB_FRIEND_CONDITION;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	// Memo : 상대가 나를 삭제했거나 차단한 상태이면 이동이 취소된다.
	//
	pos = pCHAR_FRIEND->m_mapFriend.find( pMyChar->m_szName );
	if ( pos==pCHAR_FRIEND->m_mapFriend.end() )
	{
		NetMsgFB.emFB = EM2FRIEND_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}	

	SFRIEND &sFRIEND2 = (*pos).second;
//	if( !pMe ) return FALSE;

	if( sFRIEND2.IsBLOCK() )
	{
		NetMsgFB.emFB = EM2FRIEND_FB_FRIEND_BLOCK;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if( sFRIEND2.IsOFF() )
	{
		NetMsgFB.emFB = EM2FRIEND_FB_FRIEND_OFF;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pCHAR_FRIEND->m_dwCurFieldSvr==FIELDSERVER_MAX )
	{
		NetMsgFB.emFB = EM2FRIEND_FB_FRIEND_CONDITION;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pCHAR_FRIEND->m_nChannel!=pMyChar->m_nChannel )
	{
		NetMsgFB.emFB = EM2FRIEND_FB_FRIEND_CHANNEL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Note : 친구의 위치 점검 요청.
	GLMSG::SNETPC_2_FRIEND_CK NetMsgCK;
	NetMsgCK.dwCOMMAND_CHARID = pMyChar->m_dwCharID;
	NetMsgCK.dwFRIEND_CHARID = pCHAR_FRIEND->m_dwCharID;
	NetMsgCK.wItemPosX = pNetMsg->wItemPosX;
	NetMsgCK.wItemPosY = pNetMsg->wItemPosY;
	SENDTOFIELDSVR ( pCHAR_FRIEND->m_nChannel, pCHAR_FRIEND->m_dwCurFieldSvr, &NetMsgCK );


	return TRUE;
}




// *****************************************************
// Desc: 지정한 캐릭터의 귓속말을 엿듣는다.
// *****************************************************
BOOL GLAgentServer::MsgGetWhisperMsg( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_GM_GETWHISPERMSG* pNetMsg = (GLMSG::SNETPC_GM_GETWHISPERMSG*) nmg;

	PGLCHARAG pMyChar = GetChar( dwGaeaID );
	if( !pMyChar ) return FALSE;

	if( pMyChar->m_dwUserLvl < USER_GM1 )
	{
		return FALSE;
	}


	if( strlen( pNetMsg->szNAME ) != 0 )
	{
		PGLCHARAG pTo_CHAR;
		pTo_CHAR = GetChar( pNetMsg->szNAME );
		if( pTo_CHAR )
		{		
			UINT i; 
			bool bAdd = TRUE;
			for( i = 0; i < pTo_CHAR->m_vecGetWhisperList.size(); i++ )
			{
				if( dwClientID == pTo_CHAR->m_vecGetWhisperList[i].dwClientID &&
					dwGaeaID == pTo_CHAR->m_vecGetWhisperList[i].dwGaeaID )
				{
					bAdd = FALSE; break;
				}
			}
			if( bAdd )
			{
				SGETWHISPERLIST WhisperList;
				WhisperList.dwClientID = dwClientID;
				WhisperList.dwGaeaID   = dwGaeaID;
				pTo_CHAR->m_vecGetWhisperList.push_back( WhisperList );

				pMyChar->m_GmWhisperList.dwGaeaID   = pTo_CHAR->m_dwGaeaID;
				pMyChar->m_GmWhisperList.dwClientID = pTo_CHAR->m_dwClientID;
				pMyChar->m_bViewWhisperMSG			= TRUE;
			}
		}
	}else{
		pMyChar->m_bViewWhisperMSG = FALSE;
	}
	return TRUE;
}

// *****************************************************
// Desc: 지정된 케릭터 위치로 이동
// *****************************************************
BOOL GLAgentServer::MsgMove2Char ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_GM_MOVE2CHAR* pNetMsg = (GLMSG::SNETPC_GM_MOVE2CHAR*) nmg;
	GLMSG::SNETPC_GM_MOVE2CHAR_FB NetMsgFB;

	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;

	if ( pMyChar->m_dwUserLvl < USER_GM3 )
	{
		NetMsgFB.emFB = EMGM_MOVE2CHAR_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	PGLCHARAG pTO_CHAR;;

	if ( pNetMsg->dwCHARID != 0 )
	{
		pTO_CHAR = GetCharID ( pNetMsg->dwCHARID );
	}
	else
	{
		pTO_CHAR = GetChar ( pNetMsg->szNAME );
	}

	if ( !pTO_CHAR )
	{
		NetMsgFB.emFB = EMGM_MOVE2CHAR_FB_FAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pTO_CHAR->m_dwCurFieldSvr==FIELDSERVER_MAX )
	{
		NetMsgFB.emFB = EMGM_MOVE2CHAR_FB_TO_CONDITION;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pTO_CHAR->m_nChannel!=pMyChar->m_nChannel )
	{
		NetMsgFB.emFB = EMGM_MOVE2CHAR_FB_CHANNEL;
		NetMsgFB.nChannel = pTO_CHAR->m_nChannel;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	Note : 친구의 위치 점검 요청.
	GLMSG::SNETPC_GM_MOVE2CHAR_POS NetMsgCK;
	NetMsgCK.dwCOMMAND_CHARID = pMyChar->m_dwCharID;
	NetMsgCK.dwTO_CHARID = pTO_CHAR->m_dwCharID;
	SENDTOFIELDSVR ( pTO_CHAR->m_nChannel, pTO_CHAR->m_dwCurFieldSvr, &NetMsgCK );

	return TRUE;
}

// *****************************************************
// Desc: 유저 채팅 블럭 처리 (User Account)
// *****************************************************
BOOL GLAgentServer::MsgGmChatBlockUA ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_CHAT_BLOCK_UACCOUNT *pNetMsg = (GLMSG::SNET_GM_CHAT_BLOCK_UACCOUNT *) nmg;
	GLMSG::SNET_GM_CHAT_BLOCK_FB NetMsgFB;

	PGLCHARAG pCHAR = GetCharUA ( pNetMsg->szUACCOUNT );
	// 접속중이 아닐경우
	if ( !pCHAR )
	{
		NetMsgFB.bBLOCK = false;
		StringCchCopy ( NetMsgFB.szUACCOUNT, USR_ID_LENGTH+1, pNetMsg->szUACCOUNT );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );

		// 채팅블럭 예약자 DB에 저장한다.

		return FALSE;
	}

	if ( pNetMsg->dwBLOCK_MINUTE!=0 )
	{
		//	블럭시간 지정.
		CTime cBLOCK = CTime::GetCurrentTime();
		CTimeSpan cSPAN(0,0,pNetMsg->dwBLOCK_MINUTE,0);
		cBLOCK += cSPAN;

		pCHAR->m_tCHATBLOCK = cBLOCK.GetTime();
	}
	else
	{
		//	블럭시간 해지.
		pCHAR->m_tCHATBLOCK = 0;
	}

	//	DB에 저장.
	CSetChatBlockTime *pDbAction = new CSetChatBlockTime(pCHAR->m_dwUserID,pCHAR->m_tCHATBLOCK);
	if ( GetDBMan() )	GetDBMan()->AddJob ( pDbAction );

	//	FLD에 알림.
	GLMSG::SNET_GM_CHAT_BLOCK_FLD NetMsgFLD;
	NetMsgFLD.dwCHARID = pCHAR->m_dwCharID;
	NetMsgFLD.dwBLOCK_MINUTE = pNetMsg->dwBLOCK_MINUTE;
	SENDTOALLCHANNEL(&NetMsgFLD);

	//	FB.
	NetMsgFB.bBLOCK = true;
	NetMsgFB.dwBLOCK_MINUTE = pNetMsg->dwBLOCK_MINUTE;
	StringCchCopy ( NetMsgFB.szCHARNAME, CHAR_SZNAME, pCHAR->m_szName );
	StringCchCopy ( NetMsgFB.szUACCOUNT, USR_ID_LENGTH+1, pCHAR->m_szUserName );
	SENDTOCLIENT ( dwClientID, &NetMsgFB );

	return TRUE;
}

// *****************************************************
// Desc: 유저 채팅 블럭 처리 (캐릭터명)
// *****************************************************
BOOL GLAgentServer::MsgGmChatBlockCN ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_CHAT_BLOCK_CHARNAME *pNetMsg = (GLMSG::SNET_GM_CHAT_BLOCK_CHARNAME *) nmg;
	GLMSG::SNET_GM_CHAT_BLOCK_FB NetMsgFB;

	PGLCHARAG pCHAR = GetChar ( pNetMsg->szCHARNAME );
	// 접속중이 아닐경우
	if ( !pCHAR )
	{
		NetMsgFB.bBLOCK = false;
		StringCchCopy ( NetMsgFB.szUACCOUNT, USR_ID_LENGTH+1, pNetMsg->szCHARNAME );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );

		// 채팅블럭 예약자 DB에 저장한다.
		

		return FALSE;
	}

	if ( pNetMsg->dwBLOCK_MINUTE!=0 )
	{
		//	블럭시간 지정.
		CTime cBLOCK = CTime::GetCurrentTime();
		CTimeSpan cSPAN(0,0,pNetMsg->dwBLOCK_MINUTE,0);
		cBLOCK += cSPAN;

		pCHAR->m_tCHATBLOCK = cBLOCK.GetTime();
	}
	else
	{
		//	블럭시간 해지.
		pCHAR->m_tCHATBLOCK = 0;
	}

	//	DB에 저장.
	CSetChatBlockTime *pDbAction = new CSetChatBlockTime(pCHAR->m_dwUserID,pCHAR->m_tCHATBLOCK);
	if ( GetDBMan() )	GetDBMan()->AddJob ( pDbAction );

	//	FLD에 알림.
	GLMSG::SNET_GM_CHAT_BLOCK_FLD NetMsgFLD;
	NetMsgFLD.dwCHARID = pCHAR->m_dwCharID;
	NetMsgFLD.dwBLOCK_MINUTE = pNetMsg->dwBLOCK_MINUTE;
	SENDTOALLCHANNEL(&NetMsgFLD);

	//	FB.
	NetMsgFB.bBLOCK = true;
	NetMsgFB.dwBLOCK_MINUTE = pNetMsg->dwBLOCK_MINUTE;
	StringCchCopy ( NetMsgFB.szCHARNAME, CHAR_SZNAME, pCHAR->m_szName );
	StringCchCopy ( NetMsgFB.szUACCOUNT, USR_ID_LENGTH+1, pCHAR->m_szUserName );
	SENDTOCLIENT ( dwClientID, &NetMsgFB );

	return TRUE;
}

BOOL GLAgentServer::MsgGmChatBlockCI ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_CHAT_BLOCK_CHARID *pNetMsg = (GLMSG::SNET_GM_CHAT_BLOCK_CHARID *) nmg;
	GLMSG::SNET_GM_CHAT_BLOCK_FB NetMsgFB;

	PGLCHARAG pCHAR = GetCharID ( pNetMsg->dwCHARID );
	if ( !pCHAR )
	{
		NetMsgFB.bBLOCK = false;
		StringCchCopy ( NetMsgFB.szUACCOUNT, USR_ID_LENGTH+1, "notfound" );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	if ( pNetMsg->dwBLOCK_MINUTE!=0 )
	{
		//	블럭시간 지정.
		pCHAR->m_tCHATBLOCK;

		CTime cBLOCK = CTime::GetCurrentTime();
		CTimeSpan cSPAN(0,0,pNetMsg->dwBLOCK_MINUTE,0);
		cBLOCK += cSPAN;

		pCHAR->m_tCHATBLOCK = cBLOCK.GetTime();
	}
	else
	{
		//	블럭시간 해지.
		pCHAR->m_tCHATBLOCK = 0;
	}

	//	DB에 저장.
	CSetChatBlockTime *pDbAction = new CSetChatBlockTime(pCHAR->m_dwUserID,pCHAR->m_tCHATBLOCK);
	if ( GetDBMan() )	GetDBMan()->AddJob ( pDbAction );

	//	FLD에 알림.
	GLMSG::SNET_GM_CHAT_BLOCK_FLD NetMsgFLD;
	NetMsgFLD.dwCHARID = pCHAR->m_dwCharID;
	NetMsgFLD.dwBLOCK_MINUTE = pNetMsg->dwBLOCK_MINUTE;
	SENDTOALLCHANNEL(&NetMsgFLD);

	//	FB.
	NetMsgFB.bBLOCK = true;
	NetMsgFB.dwBLOCK_MINUTE = pNetMsg->dwBLOCK_MINUTE;
	StringCchCopy ( NetMsgFB.szCHARNAME, CHAR_SZNAME, pCHAR->m_szName );
	StringCchCopy ( NetMsgFB.szUACCOUNT, USR_ID_LENGTH+1, pCHAR->m_szUserName );
	SENDTOCLIENT ( dwClientID, &NetMsgFB );

	return TRUE;
}

BOOL GLAgentServer::MsgGmWarningMSG ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM1 )	return FALSE;

	GLMSG::SNET_GM_WARNING_MSG *pNetMsg = (GLMSG::SNET_GM_WARNING_MSG*) nmg;
	GLMSG::SNET_GM_WARNING_MSG_FLD NetMsgBrd;

	NetMsgBrd.dwGaeaID = dwGaeaID;
	NetMsgBrd.bOn = pNetMsg->bOn;
	SENDTOALLCHANNEL ( &NetMsgBrd );

	return TRUE;
}

BOOL GLAgentServer::MsgGmShowMeTheMoney ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
#if defined(_RELEASED) || defined(KRT_PARAM)
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_SHOWMETHEMONEY *pNetMsg = (GLMSG::SNET_GM_SHOWMETHEMONEY*) nmg;
	GLMSG::SNET_GM_SHOWMETHEMONEY_FLD NetMsgBrd;

	NetMsgBrd.dwGaeaID = dwGaeaID;
	NetMsgBrd.llMoney = pNetMsg->llMoney;
	SENDTOALLCHANNEL( &NetMsgBrd );
#endif

	return TRUE;
}

BOOL GLAgentServer::MsgUserCharInfo4NAME ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;

	GLMSG::SNET_USER_CHAR_INFO_4NAME *pNetMsg = (GLMSG::SNET_USER_CHAR_INFO_4NAME *) nmg;
	GLMSG::SNET_USER_CHAR_INFO_AGT_FB NetMsgFB;

	PGLCHARAG pCHAR = GetChar ( pNetMsg->szCHARNAME );
	if ( !pCHAR )
	{
		StringCchCopy ( NetMsgFB.szCHARNAME, CHAR_SZNAME, "not found" );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );

		return FALSE;
	}

	//	필드에 상세 정보 요청.
	GLMSG::SNET_USER_CHAR_INFO_FLD NetMsgFld;
	NetMsgFld.dwCHARID = pCHAR->m_dwCharID;
	NetMsgFld.dwCLIENTID_FB = dwClientID;
	SENDTOALLCHANNEL( &NetMsgFld );

	return TRUE;
}

BOOL GLAgentServer::MsgGmCharInfo4NAME ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_CHAR_INFO_4NAME *pNetMsg = (GLMSG::SNET_GM_CHAR_INFO_4NAME *) nmg;
	GLMSG::SNET_GM_CHAR_INFO_AGT_FB NetMsgFB;

	PGLCHARAG pCHAR = GetChar ( pNetMsg->szCHARNAME );
	if ( !pCHAR )
	{
		StringCchCopy ( NetMsgFB.szCHARNAME, CHAR_SZNAME, "notfound" );
		StringCchCopy ( NetMsgFB.szUACCOUNT, USR_ID_LENGTH+1, "notfound" );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	클라이언트에 agt fb
	StringCchCopy ( NetMsgFB.szCHARNAME, CHAR_SZNAME, pCHAR->m_szName );
	StringCchCopy ( NetMsgFB.szUACCOUNT, USR_ID_LENGTH+1, pCHAR->m_szUserName );
	NetMsgFB.dwSERVER;
	NetMsgFB.dwCHANNEL = (DWORD) pCHAR->m_nChannel;
	NetMsgFB.dwCHARID = pCHAR->m_dwCharID;
	NetMsgFB.dwGAEAID = pCHAR->m_dwGaeaID;
	SENDTOCLIENT ( dwClientID, &NetMsgFB );

	//	필드에 상세 정보 요청.
	GLMSG::SNET_GM_CHAR_INFO_FLD NetMsgFld;
	NetMsgFld.dwCHARID = pCHAR->m_dwCharID;
	NetMsgFld.dwCLIENTID_FB = dwClientID;
	SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}

BOOL GLAgentServer::MsgGmCharInfo4CID ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar ( dwGaeaID );
	if ( !pMyChar )							return FALSE;
	if ( pMyChar->m_dwUserLvl < USER_GM3 )	return FALSE;

	GLMSG::SNET_GM_CHAR_INFO_4CHARID *pNetMsg = (GLMSG::SNET_GM_CHAR_INFO_4CHARID *) nmg;
	GLMSG::SNET_GM_CHAR_INFO_AGT_FB NetMsgFB;

	PGLCHARAG pCHAR = GetCharID ( pNetMsg->dwCHARID );
	if ( !pCHAR )
	{
		StringCchCopy ( NetMsgFB.szCHARNAME, CHAR_SZNAME, "notfound" );
		StringCchCopy ( NetMsgFB.szUACCOUNT, USR_ID_LENGTH+1, "notfound" );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	//	클라이언트에 agt fb
	StringCchCopy ( NetMsgFB.szCHARNAME, CHAR_SZNAME, pCHAR->m_szName );
	StringCchCopy ( NetMsgFB.szUACCOUNT, USR_ID_LENGTH+1, pCHAR->m_szUserName );
	NetMsgFB.dwSERVER;
	NetMsgFB.dwCHANNEL = (DWORD) pCHAR->m_nChannel;
	NetMsgFB.dwCHARID = pCHAR->m_dwCharID;
	NetMsgFB.dwGAEAID = pCHAR->m_dwGaeaID;
	SENDTOCLIENT ( dwClientID, &NetMsgFB );

	//	필드에 상세 정보 요청.
	GLMSG::SNET_GM_CHAR_INFO_FLD NetMsgFld;
	NetMsgFld.dwCHARID = pCHAR->m_dwCharID;
	NetMsgFld.dwCLIENTID_FB = dwClientID;
	SENDTOALLCHANNEL ( &NetMsgFld );

	return TRUE;
}

BOOL GLAgentServer::MsgGmCharInfoFldFB ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNET_GM_CHAR_INFO_FLD_FB *pNetMsg = (GLMSG::SNET_GM_CHAR_INFO_FLD_FB *) nmg;

	SENDTOCLIENT ( pNetMsg->dwCLIENTID_FB, pNetMsg );

	return TRUE;
}

BOOL GLAgentServer::MsgUserCharInfoFldFB ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNET_USER_CHAR_INFO_FLD_FB *pNetMsg = (GLMSG::SNET_USER_CHAR_INFO_FLD_FB *) nmg;

	SENDTOCLIENT ( pNetMsg->dwCLIENTID_FB, pNetMsg );

	return TRUE;
}

// *****************************************************
// Desc: 케릭터 이동을 필드서버에 요청
// *****************************************************
BOOL GLAgentServer::MsgMove2CharAG ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNETPC_GM_MOVE2CHAR_AG *pNetMsg = (GLMSG::SNETPC_GM_MOVE2CHAR_AG *) nmg;
	GLMSG::SNETPC_GM_MOVE2CHAR_FB NetMsgFB;

	PGLCHARAG pCHAR = GetCharID ( pNetMsg->dwCOMMAND_CHARID );
	if ( !pCHAR )	return FALSE;

	if ( pNetMsg->emFB==EMGM_MOVE2CHAR_FB_OK )
	{
		GLMSG::SNETPC_GM_MOVE2CHAR_FLD NetMsgFld;
		NetMsgFld.sToMapID = pNetMsg->sToMapID;
		NetMsgFld.vToPos = pNetMsg->vToPos;
		SENDTOFIELD ( pCHAR->m_dwClientID, &NetMsgFld );
	}
	else
	{
		//	Note : 친구에게 이동 실패 반환.
		NetMsgFB.emFB = pNetMsg->emFB;
		SENDTOCLIENT ( pCHAR->m_dwClientID, &NetMsgFB );
	}

	return TRUE;
}

BOOL GLAgentServer::MsgClubBattleRemainTime ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNETPC_SERVER_CLUB_BATTLE_REMAIN_AG* pNetMsg = 
		(GLMSG::SNETPC_SERVER_CLUB_BATTLE_REMAIN_AG*) nmg;

	PGLCHARAG pChar = GetChar ( pNetMsg->dwGaeaID );
	if ( pChar )
	{
		GLMSG::SNET_CLUB_BATTLE_REMAIN_BRD NetMsgBrd;
		NetMsgBrd.dwTime = GLGuidanceAgentMan().GetInstance().GetRemainTime ();
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgBrd );
	}

	return TRUE;
}

BOOL GLAgentServer::MsgClubDeathMatchRemainTime ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNETPC_SERVER_CLUB_DEATHMATCH_REMAIN_AG* pNetMsg = 
		(GLMSG::SNETPC_SERVER_CLUB_DEATHMATCH_REMAIN_AG*) nmg;

	PGLCHARAG pChar = GetChar ( pNetMsg->dwGaeaID );
	if ( pChar )
	{
		GLMSG::SNET_CLUB_DEATHMATCH_REMAIN_BRD NetMsgBrd;
		NetMsgBrd.dwTime = GLClubDeathMatchAgentMan().GetInstance().GetRemainTime ();
		SENDTOCLIENT ( pChar->m_dwClientID, &NetMsgBrd );
	}

	return TRUE;
}



BOOL GLAgentServer::MsgClubCDCertifyIngBrd ( NET_MSG_GENERIC* nmg )
{
	SENDTOALLCLIENT ( nmg );

	return TRUE;
}

BOOL GLAgentServer::MsgClubCDCertifyBrd ( NET_MSG_GENERIC* nmg )
{
	SENDTOALLCLIENT ( nmg );

	return TRUE;
}

BOOL GLAgentServer::MsgMarketOpenCheck ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyChar = GetChar(dwGaeaID);

	if( pMyChar == NULL )
	{
		m_pConsoleMsg->Write( LOG_TEXT_CONSOLE, "%%%% Could not found Character..%d %%%%", dwGaeaID );
	}

	return TRUE;
}

BOOL GLAgentServer::MsgClubBattleKillUpdate ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNET_CLUB_BATTLE_KILL_UPDATE_AGT* pNetMsg = (GLMSG::SNET_CLUB_BATTLE_KILL_UPDATE_AGT*) nmg;

	GLCLUB* pClub = m_cClubMan.GetClub ( pNetMsg->dwClubID );
	if ( !pClub ) return FALSE;

	pClub->KillBattleClubMem( pNetMsg->dwBattleClubID, pNetMsg->wKillPoint );
	pClub->DeathBattleClubMem( pNetMsg->dwBattleClubID, pNetMsg->wDeathPoint );

	return TRUE;
}

BOOL GLAgentServer::MsgClubBattleLastKillUpdate( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNET_CLUB_BATTLE_LAST_KILL_UPDATE_AGT* pNetMsg = (GLMSG::SNET_CLUB_BATTLE_LAST_KILL_UPDATE_AGT*) nmg;

	GLCLUB* pClub = m_cClubMan.GetClub ( pNetMsg->dwClubID_A );
	if ( !pClub ) return FALSE;

	pClub->SetLastBattlePoint( pNetMsg->dwClubID_B, pNetMsg->wKillPoint, pNetMsg->wDeathPoint );

	return TRUE;
}

BOOL GLAgentServer::MsgReqTaxi ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_REQ_TAXI* pNetMsg = (GLMSG::SNETPC_REQ_TAXI*)nmg;

	GLMSG::SNETPC_REQ_TAXI_FB NetMsgFB;

	PGLCHARAG pMyChar = GetChar(dwGaeaID);
	if ( !pMyChar ) return FALSE;

	// 정류장 검사
	STAXI_MAP* pTaxiMap = GLTaxiStation::GetInstance().GetTaxiMap ( pNetMsg->dwSelectMap );
	if ( !pTaxiMap )
	{
		NetMsgFB.emFB = EMTAXI_TAKE_MAPFAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	// 정류장 검사
	STAXI_STATION* pStation = pTaxiMap->GetStation( pNetMsg->dwSelectStop );
	if ( !pStation )
	{
		NetMsgFB.emFB = EMTAXI_TAKE_STATIONFAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}
	
	SNATIVEID sMAPID(pStation->dwMAPID);

	if ( sMAPID==SNATIVEID(false) )
	{
		NetMsgFB.emFB = EMTAXI_TAKE_MAPFAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	DWORD dwFieldServer = GetFieldServer( sMAPID );
	if ( dwFieldServer == FIELDSERVER_MAX )
	{
		NetMsgFB.emFB = EMTAXI_TAKE_MAPFAIL;
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	// 같은 필드이면 그냥 보내고
	// 다른 필드이면 좌표를 구해서 보내준다.
	if ( dwFieldServer == pMyChar->m_dwCurFieldSvr )
	{
		SENDTOFIELD( dwClientID, pNetMsg );
	}
	else
	{
		GLMSG::SNETPC_REQ_TAXI_NPCPOS NetMsgPos;
		NetMsgPos.dwSelectMap = pNetMsg->dwSelectMap;
		NetMsgPos.dwSelectStop = pNetMsg->dwSelectStop;
		NetMsgPos.dwGaeaID = pNetMsg->dwGaeaID;
		NetMsgPos.wPosX = pNetMsg->wPosX;
		NetMsgPos.wPosY = pNetMsg->wPosY;

		SENDTOFIELDSVR( pMyChar->m_nChannel, dwFieldServer, &NetMsgPos );
	}

	return TRUE;
}

BOOL GLAgentServer::MsgReqTaxiNpcPos ( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNETPC_REQ_TAXI_NPCPOS_FB* pNetMsg = (GLMSG::SNETPC_REQ_TAXI_NPCPOS_FB*)nmg;
	GLMSG::SNETPC_REQ_TAXI_FB NetMsgFB;

	PGLCHARAG pMyChar = GetChar(pNetMsg->dwGaeaID);
	if ( !pMyChar )
	{
		return FALSE;
	}

	if ( pNetMsg->vPos == D3DXVECTOR3(FLT_MAX,FLT_MAX,FLT_MAX) )
	{
		NetMsgFB.emFB = EMTAXI_TAKE_NPCFAIL;
		SENDTOCLIENT ( pMyChar->m_dwClientID, &NetMsgFB );
		return FALSE;
	}
	


	GLMSG::SNETPC_REQ_TAXI NetMsg;
	NetMsg.dwSelectMap = pNetMsg->dwSelectMap;
	NetMsg.dwSelectStop = pNetMsg->dwSelectStop;
	NetMsg.wPosX = pNetMsg->wPosX;
	NetMsg.wPosY = pNetMsg->wPosY;
	NetMsg.dwGaeaID = pNetMsg->dwGaeaID;
	NetMsg.vPos = pNetMsg->vPos;
	
	SENDTOFIELD( pMyChar->m_dwClientID, &NetMsg );

	return TRUE;
}

BOOL GLAgentServer::MsgNpcCommission( NET_MSG_GENERIC* nmg, DWORD dwClientID)
{
	GLMSG::SNET_INVEN_NPC_COMMISSION* pNetMsg = (GLMSG::SNET_INVEN_NPC_COMMISSION*)nmg;
	GLMSG::SNET_INVEN_NPC_COMMISSION_FB NetMsgFB;

	NetMsgFB.dwCharID = pNetMsg->dwCharID;
	NetMsgFB.dwUserID = pNetMsg->dwUserID;
	NetMsgFB.lnCommission = pNetMsg->lnCommission;

	PGLCHARAG pChar = GetCharID(pNetMsg->dwCharID);	

	//	캐릭터가 접속중일때 ( 타필드에 있는 상황 ) 
	//	비접속중일때 필드에 저장을 요청한다.
	if ( pChar )
	{
		NetMsgFB.emFB = EMREQ_NPC_COMMISSION_CHAR;		
		SENDTOFIELDSVR( pChar->m_nChannel, pChar->m_dwCurFieldSvr, &NetMsgFB );
	}
	else
	{
		NetMsgFB.emFB = EMREQ_NPC_COMMISSION_DB;			
		SENDTOFIELDSVR(  pNetMsg->nCHANNEL, pNetMsg->dwFieldID, &NetMsgFB );
	}
	
	return TRUE;
}

HRESULT GLAgentServer::MsgProcess ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	switch ( nmg->nType )
	{
	default:										break;
	case NET_MSG_GCTRL_REQ_CLOSE_CLIENT_AGT:			MsgReqCloseClientAgt(nmg);							break;
	case NET_MSG_GCTRL_REQ_GATEOUT:					MsgReqGateOut ( nmg, dwClientID, dwGaeaID ); break;
	case NET_MSG_GCTRL_REQ_GENITEM_AGT:				MsgReqGenItemAgt ( nmg, dwClientID ); break;
	case NET_MSG_FIELDSVR_CHARCHK_FB:				MsgReqFieldSvrCharChkFb ( nmg, dwClientID, dwGaeaID ); break;
	case NET_MSG_SERVER_GENERALCHAT:				MsgServerGeneralChat ( nmg ); break;
	
	case NET_MSG_GCTRL_CONFRONTPTY_CHECKMBR_AGT:	MsgConfrontPtyCheckMber ( nmg ); break;
	case NET_MSG_GCTRL_CONFRONTCLB_CHECKMBR_AGT:	MsgConfrontClbCheckMber ( nmg ); break;

	case NET_MSG_GCTRL_CLUB_MEMBER_DB2DEL:			MsgClubMemberDb2Del ( nmg, dwClientID, dwGaeaID ); break;

	case NET_MSG_GCTRL_CLUB_CERTIFIED_AGT:			MsgClubCertified ( nmg, dwClientID, dwGaeaID ); break;
	case NET_MSG_GCTRL_CLUB_INCOME_MONEY_AGT:		MsgClubIncomeMoney ( nmg ); break;
	case NET_MSG_GCTRL_CLUB_INCOME_UP:				MsgClubIncomeMoneyUp ( nmg ); break;
	//case NET_MSG_GCTRL_CLUB_INCOME_FB:				MsgClubIncomeMoneyFb ( nmg ); break;
	case NET_MSG_GCTRL_2_FRIEND_AG:					Msg2FriendAG ( nmg ); break;
	case NET_MSG_GM_MOVE2CHAR_AG:					MsgMove2CharAG ( nmg ); break;
	case NET_MSG_GM_CHAR_INFO_FLD_FB:				MsgGmCharInfoFldFB ( nmg );	break;
	case NET_MSG_USER_CHAR_INFO_FLD_FB:				MsgUserCharInfoFldFB ( nmg );	break;
	case NET_MSG_GCTRL_CLUB_CD_CERTIFY_ING_BRD:		MsgClubCDCertifyIngBrd ( nmg ); break;
	case NET_MSG_GCTRL_CLUB_CD_CERTIFY_BRD:			MsgClubCDCertifyBrd ( nmg ); break;
	case NET_MSG_GCTRL_CLUB_BATTLE_KILL_UPDATE_AGT:	MsgClubBattleKillUpdate ( nmg ); break;
	case NET_MSG_GCTRL_CLUB_BATTLE_LAST_KILL_UPDATE_AGT:	MsgClubBattleLastKillUpdate ( nmg ); break;
	case NET_MSG_GCTRL_REQ_TAXI_NPCPOS_FB:			MsgReqTaxiNpcPos( nmg );			break;
	case NET_MSG_GCTRL_CREATE_INSTANT_MAP_REQ:		MsgReqCreateInstantMap ( nmg, dwClientID, dwGaeaID ); break;
	case NET_MSG_GCTRL_CREATE_INSTANT_MAP_DEL:		MsgReqDeleteInstantMap ( nmg ); break;
	case NET_MSG_GCTRL_NPC_COMMISSION:				MsgNpcCommission( nmg, dwClientID );	break;

		/*activity system, Juver, 2017/10/30 */
	case NET_MSG_GCTRL_ACTIVITY_NOTIFY_AGENT:		MsgActivityBroadcast( nmg );	break;



	case NET_MSG_EMPTYMSG:
		{
			GLMSG::SNET_EMPTYMSG *pNetMsg = (GLMSG::SNET_EMPTYMSG*) nmg;
			SENDTOALLCHANNEL( &pNetMsg );
		}
		
		break;

	case NET_MSG_ALLIANCE_BATTLE_BEGIN:
		{
			GLMSG::SNET_ALLIANCE_BATTLE_BEGIN *pNetMsg1 = (GLMSG::SNET_ALLIANCE_BATTLE_BEGIN*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg1 );
		}
		break;
	case NET_MSG_ALLIANCE_BATTLE_BEGIN_REFUSE:
		{
			GLMSG::SNET_ALLIANCE_BATTLE_BEGIN_REFUSE *pNetMsg2 = (GLMSG::SNET_ALLIANCE_BATTLE_BEGIN_REFUSE*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg2 );
		}
		break;
	case NET_MSG_ALLIANCE_BATTLE_OVER_ARMISTICE:
		{
			GLMSG::SNET_ALLIANCE_BATTLE_OVER_ARMISTICE *pNetMsg3 = (GLMSG::SNET_ALLIANCE_BATTLE_OVER_ARMISTICE*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg3 );
		}
		break;
	case NET_MSG_ALLIANCE_BATTLE_OVER_ARMISTICE_RESULT:
		{
			GLMSG::SNET_ALLIANCE_BATTLE_OVER_ARMISTICE_RESULT *pNetMsg4 = (GLMSG::SNET_ALLIANCE_BATTLE_OVER_ARMISTICE_RESULT*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg4 );
		}
		break;
	case NET_MSG_ALLIANCE_BATTLE_OVER_DRAW:
		{
			GLMSG::SNET_ALLIANCE_BATTLE_OVER_DRAW *pNetMsg5 = (GLMSG::SNET_ALLIANCE_BATTLE_OVER_DRAW*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg5 );
		}
		break;
	case NET_MSG_ALLIANCE_BATTLE_OVER_SUBMISSION:
		{
			GLMSG::SNET_ALLIANCE_BATTLE_OVER_SUBMISSION *pNetMsg6 = (GLMSG::SNET_ALLIANCE_BATTLE_OVER_SUBMISSION*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg6 );
		}
		break;
	case NET_MSG_ALLIANCE_BATTLE_OVER_WIN:
		{
			GLMSG::SNET_ALLIANCE_BATTLE_OVER_WIN *pNetMsg7 = (GLMSG::SNET_ALLIANCE_BATTLE_OVER_WIN*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg7 );

		}
		break;
	case NET_MSG_ALLIANCE_BATTLE_RESULT:
		{
			GLMSG::SNET_ALLIANCE_BATTLE_RESULT *pNetMsg8 = (GLMSG::SNET_ALLIANCE_BATTLE_RESULT*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg8 );
		}
		break;
	case NET_MSG_CLUB_BATTLE_BEGIN:
		{
			GLMSG::SNET_CLUB_BATTLE_BEGIN *pNetMsg9 = (GLMSG::SNET_CLUB_BATTLE_BEGIN*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg9 );
		}
		break;
	case NET_MSG_CLUB_BATTLE_BEGIN_REFUSE:
		{
			GLMSG::SNET_CLUB_BATTLE_BEGIN_REFUSE *pNetMsg10 = (GLMSG::SNET_CLUB_BATTLE_BEGIN_REFUSE*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg10 );

		}
		break;
	case NET_MSG_CLUB_BATTLE_OVER_ARMISTICE:
		{
			GLMSG::SNET_CLUB_BATTLE_OVER_ARMISTICE *pNetMsg11 = (GLMSG::SNET_CLUB_BATTLE_OVER_ARMISTICE*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg11 );

		}
		break;
	case NET_MSG_CLUB_BATTLE_OVER_ARMISTICE_RESULT:
		{
			GLMSG::SNET_CLUB_BATTLE_OVER_ARMISTICE_RESULT *pNetMsg12 = (GLMSG::SNET_CLUB_BATTLE_OVER_ARMISTICE_RESULT*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg12 );

		}
		break;
	case NET_MSG_CLUB_BATTLE_OVER_SUBMISSION:
		{
			GLMSG::SNET_CLUB_BATTLE_OVER_SUBMISSION *pNetMsg13 = (GLMSG::SNET_CLUB_BATTLE_OVER_SUBMISSION*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg13 );

		}
		break;
	case NET_MSG_CLUB_BATTLE_OVER_WIN:
		{
			GLMSG::SNET_CLUB_BATTLE_OVER_WIN *pNetMsg14 = (GLMSG::SNET_CLUB_BATTLE_OVER_WIN*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg14 );

		}
		break;
	case NET_MSG_CLUB_BATTLE_RESULT:
		{
			GLMSG::SNET_CLUB_BATTLE_RESULT *pNetMsg15 = (GLMSG::SNET_CLUB_BATTLE_RESULT*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg15 );

		}
		break;
	case NET_MSG_EMCONFRONT_END_CDRAWN:
		{
			GLMSG::SNET_EMCONFRONT_END_CDRAWN *pNetMsg16 = (GLMSG::SNET_EMCONFRONT_END_CDRAWN*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg16 );

		}
		break;
	case NET_MSG_EMCONFRONT_END_CWIN:
		{
			GLMSG::SNET_EMCONFRONT_END_CWIN *pNetMsg17 = (GLMSG::SNET_EMCONFRONT_END_CWIN*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg17 );

		}
		break;
	case NET_MSG_EMCONFRONT_END_PDRAWN:
		{
			GLMSG::SNET_EMCONFRONT_END_PDRAWN *pNetMsg18 = (GLMSG::SNET_EMCONFRONT_END_PDRAWN*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg18 );

		}
		break;
	case NET_MSG_EMCONFRONT_END_PWIN:
		{
			GLMSG::SNET_EMCONFRONT_END_PWIN *pNetMsg19 = (GLMSG::SNET_EMCONFRONT_END_PWIN*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg19 );
		}
		break;
	case NET_MSG_EMCONFRONT_START_PARTY:
		{
			GLMSG::SNET_EMCONFRONT_START_PARTY *pNetMsg20 = (GLMSG::SNET_EMCONFRONT_START_PARTY*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg20 );

		}
		break;
	case NET_MSG_EMCROWACT_KNOCK:
		{
			GLMSG::SNET_EMCROWACT_KNOCK *pNetMsg21 = (GLMSG::SNET_EMCROWACT_KNOCK*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg21 );

		}
		break;
	case NET_MSG_EMCROWACT_REPULSE:
		{
			GLMSG::SNET_EMCROWACT_REPULSE *pNetMsg22 = (GLMSG::SNET_EMCROWACT_REPULSE*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg22 );

		}
		break;
	case NET_MSG_EMGUIDCLUB_CERTIFIED:
		{
			GLMSG::SNET_EMGUIDCLUB_CERTIFIED *pNetMsg23 = (GLMSG::SNET_EMGUIDCLUB_CERTIFIED*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg23 );

		}
		break;
	case NET_MSG_BRIGHT_EVENT_MSG:
		{
			GLMSG::SNET_BRIGHT_EVENT_MSG *pNetMsg25 = (GLMSG::SNET_BRIGHT_EVENT_MSG*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg25 );

		}
		break;

	case NET_MSG_CLUB_BATTLE_OVER_DRAW:
		{
			GLMSG::SNET_CLUB_BATTLE_OVER_DRAW *pNetMsg26 = (GLMSG::SNET_CLUB_BATTLE_OVER_DRAW*) nmg;
			GLAgentServer::GetInstance().SENDTOALLCLIENT( &pNetMsg26 );

		}
		break;


	};


	/*pvp tyranny, Juver, 2017/08/24 */
	switch ( nmg->nType )
	{
	default:
		break;

	case NET_MSG_GCTRL_TYRANNY_F2A_BATTLEINFO_PC:
	case NET_MSG_GCTRL_TYRANNY_F2A_RANKINFO_PC:
	case NET_MSG_GCTRL_TYRANNY_C2A_REGISTER_REQ:
	case NET_MSG_GCTRL_TYRANNY_C2A_REJOIN_REQ:
	case NET_MSG_GCTRL_TYRANNY_C2A_MAPEXIT_REQ:
	case NET_MSG_GCTRL_TYRANNY_F2A_TOWER_CAPTURE:
	case NET_MSG_GCTRL_TYRANNY_C2A_TOWER_INFO_REQ:
	case NET_MSG_GCTRL_TYRANNY_F2A_TOWER_SCORE:
		GLPVPTyrannyAgent::GetInstance().MsgProcess( nmg, dwClientID, dwGaeaID );
		break;
	};

	/*school wars, Juver, 2018/01/19 */
	switch ( nmg->nType )
	{
	default:
		break;

	case NET_MSG_GCTRL_SCHOOLWARS_F2A_BATTLEINFO_PC:
	case NET_MSG_GCTRL_SCHOOLWARS_C2A_REGISTER_REQ:
	case NET_MSG_GCTRL_SCHOOLWARS_C2A_REJOIN_REQ:
	case NET_MSG_GCTRL_SCHOOLWARS_C2A_MAPEXIT_REQ:
	case NET_MSG_GCTRL_SCHOOLWARS_C2A_SCORE_INFO_REQ:
	case NET_MSG_GCTRL_SCHOOLWARS_F2A_SCORE_UPDATE:
	case NET_MSG_GCTRL_SCHOOLWARS_F2A_RANKINFO_PC:
		GLPVPSchoolWarsAgent::GetInstance().MsgProcess( nmg, dwClientID, dwGaeaID );
		break;
	};

	/*pvp capture the flag, Juver, 2018/01/26 */
	switch ( nmg->nType )
	{
	default:
		break;

	case NET_MSG_GCTRL_CAPTURE_THE_FLAG_F2A_BATTLEINFO_PC:
	case NET_MSG_GCTRL_CAPTURE_THE_FLAG_C2A_REGISTER_REQ:
	case NET_MSG_GCTRL_CAPTURE_THE_FLAG_C2A_REJOIN_REQ:
	case NET_MSG_GCTRL_CAPTURE_THE_FLAG_C2A_MAPEXIT_REQ:
	case NET_MSG_GCTRL_CAPTURE_THE_FLAG_F2A_RANKINFO_PC:
	case NET_MSG_GCTRL_CAPTURE_THE_FLAG_C2A_SCORE_INFO_REQ:
	case NET_MSG_GCTRL_CAPTURE_THE_FLAG_F2A_KILL_UPDATE:
	case NET_MSG_GCTRL_CAPTURE_THE_FLAG_F2A_CAPTURE_UPDATE:
	case NET_MSG_GCTRL_CAPTURE_THE_FLAG_F2A_FLAG_HOLD:
		GLPVPCaptureTheFlagAgent::GetInstance().MsgProcess( nmg, dwClientID, dwGaeaID );
		break;
	};

	if ( dwGaeaID == GAEAID_NULL )				return E_FAIL;
	if ( !GetChar(dwGaeaID) )					return E_FAIL;

	switch ( nmg->nType )
	{
	default:									break;

	case NET_MSG_REQ_FRIENDLIST:
	case NET_MSG_REQ_FRIENDADD:
	case NET_MSG_REQ_FRIENDADD_ANS:
	case NET_MSG_REQ_FRIENDDEL:
	case NET_MSG_REQ_FRIENDBLOCK:

	case NET_MSG_GCTRL_ACTSTATE:

	case NET_MSG_GCTRL_CLUB_MEMBER_REQ_2AGT:
	case NET_MSG_GCTRL_CLUB_MEMBER_DEL:
	case NET_MSG_GCTRL_CLUB_MEMBER_SECEDE:
	case NET_MSG_GCTRL_CLUB_AUTHORITY_REQ:
	case NET_MSG_GCTRL_CLUB_AUTHORITY_REQ_ANS:
	case NET_MSG_GCTRL_CLUB_MARK_INFO:
	case NET_MSG_GCTRL_CLUB_MARK_CHANGE:
	case NET_MSG_GCTRL_CLUB_RANK_2AGT:

	case NET_MSG_GCTRL_CLUB_COMMISSION:
	case NET_MSG_GCTRL_CLUB_NOTICE_REQ:
	case NET_MSG_GCTRL_CLUB_SUBMASTER:

	case NET_MSG_GCTRL_CLUB_ALLIANCE_REQ:
	case NET_MSG_GCTRL_CLUB_ALLIANCE_REQ_ANS:
	case NET_MSG_GCTRL_CLUB_ALLIANCE_DEL_REQ:
	case NET_MSG_GCTRL_CLUB_ALLIANCE_SEC_REQ:
	case NET_MSG_GCTRL_CLUB_ALLIANCE_DIS_REQ:

	case NET_MSG_GCTRL_CLUB_BATTLE_REQ:
	case NET_MSG_GCTRL_CLUB_BATTLE_REQ_ANS:
	case NET_MSG_GCTRL_CLUB_BATTLE_ARMISTICE_REQ:
	case NET_MSG_GCTRL_CLUB_BATTLE_ARMISTICE_REQ_ANS:
	case NET_MSG_GCTRL_CLUB_BATTLE_SUBMISSION_REQ:

	case NET_MSG_GCTRL_ALLIANCE_BATTLE_REQ:
	case NET_MSG_GCTRL_ALLIANCE_BATTLE_REQ_ANS:
	case NET_MSG_GCTRL_ALLIANCE_BATTLE_ARMISTICE_REQ:
	case NET_MSG_GCTRL_ALLIANCE_BATTLE_ARMISTICE_REQ_ANS:
	case NET_MSG_GCTRL_ALLIANCE_BATTLE_SUBMISSION_REQ:

	case NET_MSG_GCTRL_INVEN_RENAME_AGTBRD:
	case NET_MSG_SMS_PHONE_NUMBER_AGTBRD:
	case NET_MSG_GCTRL_UPDATE_STARTCALL:

	case NET_MSG_GCTRL_PUTON_CHANGE_AG:
	case NET_MSG_REQ_FRIEND_CLUB_OPEN:
	case NET_QBOX_OPTION_REQ_AG:
	case NET_MSG_REQ_ATTENDLIST:
	case NET_MSG_REQ_ATTENDANCE:

	//----- offline vend
	case NET_MSG_GCTRL_PMARKET_OPEN_AGT:
	//----- offline vend
		{
			PGLCHARAG pMyChar = GetChar(dwGaeaID);
			pMyChar->MsgProcess ( nmg );
		}
		break;

	case NET_MSG_CHAT:							ChatMsgProc ( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_CHAT_LOUDSPEAKER_AGT:			ChatMsgLoudSpeakerAgt ( nmg, dwClientID, dwGaeaID );break;
	case NET_MSG_GCTRL_REQ_REBIRTH:				MsgReqReBirth ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_REGEN_GATE_FB:			MsgReqReGenGate ( nmg, dwClientID, dwGaeaID );		break;

	case NET_MSG_GCTRL_FIELDSVR_OUT_FB:			MsgFieldSvrOutFb ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_REBIRTH_OUT_FB:			MsgReBirthOutFb ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_REQ_RECALL_AG:			MsgReCallAg ( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_REQ_MUST_LEAVE_MAP_AG:			MsgLvUpMustLeaveMap ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GCTRL_REQ_TELEPORT_AG:			MsgTeleportAg ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_REQ_SERVER_CLUB_BATTLE_REMAIN_AG:		MsgClubBattleRemainTime ( nmg );					break;
	case NET_MSG_GCTRL_REQ_SERVER_CLUB_DEATHMATCH_REMAIN_AG:	MsgClubDeathMatchRemainTime ( nmg );				break;

	case NET_MSG_GCTRL_PARTY_LURE:				MsgPartyLure ( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_GCTRL_PARTY_LURE_TAR_ANS:		MsgPartyLureTarAns ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GCTRL_PARTY_SECEDE:			MsgPartySecede ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_PARTY_AUTHORITY:			MsgPartyAuthority( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_PARTY_DISSOLVE:			MsgPartyDissolve ( nmg, dwClientID, dwGaeaID );		break;

	case NET_MSG_GCTRL_2_FRIEND_REQ:			Msg2FriendReq ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GM_MOVE2CHAR:					MsgMove2Char ( nmg, dwClientID, dwGaeaID );			break;

	case NET_MSG_GCTRL_REQ_GATEOUT_FB:			MsgReqGateOutFB ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_REQ_REBIRTH_FB:			MsgReqReBirthFB ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_CREATE_INSTANT_MAP_FB:		MsgReqCreateInstantMapFB ( nmg, dwClientID, dwGaeaID );		break;

	case NET_MSG_GCTRL_CONFRONT:				MsgReqConfront ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_CONFRONT_ANS:			MsgAnsConfront ( nmg, dwClientID, dwGaeaID );		break;

	case NET_MSG_GCTRL_CONFRONT_FB:				MsgAnsConfrontFB ( nmg, dwClientID, dwGaeaID );		break;

	case NET_MSG_GCTRL_CONFRONT_END2_AGT:		MsgConfrontEnd2Agt ( nmg, dwClientID, dwGaeaID );	break;

	case NET_MSG_GCTRL_CLUB_NEW_2AGT:			MsgClubNew2Agt ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_CLUB_NEW_DB2AGT:			MsgClubNewDb2Agt ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GCTRL_CLUB_DISSOLUTION:		MsgClubDissolution ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_CHARPOS_FROMDB2AGT:			GameJoinToFieldSvr ( nmg, dwClientID, dwGaeaID );	break;


	case NET_MSG_SERVER_CTRL_WEATHER:			
	case NET_MSG_SERVER_CTRL_WEATHER2:			MsgServerCtrlWeather ( nmg, dwClientID, dwGaeaID );	break;
		
	case NET_MSG_SERVER_CTRL_GENITEMHOLD:		MsgServerCtrlGenHold ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_SERVER_CTRL_TIME:				MsgServerCtrlTime ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_SERVER_CTRL_MONTH:				MsgServerCtrlMonth ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_BLOCK_DETECTED:				MsgBlockDetected ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GM_KICK_USER:					MsgGmKickUser ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GM_MOVE2GATE:					MsgGMMove2Gate ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GM_MOVE2MAPPOS:				MsgGMMove2Pos ( nmg, dwClientID, dwGaeaID );		break;

	case NET_MSG_GM_EVENT_ITEM_GEN:				MsgGmEventItemGen ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GM_EVENT_ITEM_GEN_END:			MsgGmEventItemGenEnd ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GM_EVENT_MONEY_GEN:			MsgGmEventMoneyGen ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GM_EVENT_MONEY_GEN_END:		MsgGmEventMoneyGenEnd ( nmg, dwClientID, dwGaeaID );break;
	case NET_MSG_GM_EVENT_EXP:					MsgGmEventExp( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_GM_EVENT_EXP_END:				MsgGmEventExpEnd( nmg, dwClientID, dwGaeaID );		break;
	
	case NET_MSG_GM_EVENT_EX:					MsgGmEventEx ( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_GM_EVENT_EX_END:				MsgGmEventExEnd ( nmg, dwClientID, dwGaeaID );		break;
	
	case NET_MSG_GM_VIEWWORKEVENT:				MsgGmViewWorkEvent( nmg, dwClientID, dwGaeaID );	break;

	case NET_MSG_GM_LIMIT_EVENT_BEGIN:			MsgGmLimitEventBegin ( nmg, dwClientID, dwGaeaID ); break;
	case NET_MSG_GM_LIMIT_EVENT_END:			MsgGmLimitEventEnd ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GM_LIMIT_EVENT_RESTART:		MsgGmLimitEventRestart ( nmg, dwClientID, dwGaeaID ); break;
	case NET_MSG_GM_LIMIT_EVENT_TIME_REQ:       MsgGmLimitEventTimeReq ( nmg, dwClientID, dwGaeaID ); break; 
	case NET_MSG_CYBERCAFECLASS_UPDATE:			MsgCyberCafeClassUpdate ( nmg, dwClientID, dwGaeaID ); break; 

	case NET_MSG_MARKET_CHECKTIME:				MsgMarketOpenCheck( nmg, dwClientID, dwGaeaID );	break;

	case NET_MSG_GM_BIGHEAD:					MsgGmBigHead ( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_GM_BIGHAND:					MsgGmBigHand ( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_GM_CHAT_BLOCK_UACCOUNT:		MsgGmChatBlockUA ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GM_CHAT_BLOCK_CHARNAME:		MsgGmChatBlockCN ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GM_CHAT_BLOCK_CHARID:			MsgGmChatBlockCI ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_USER_CHAR_INFO_4NAME:			MsgUserCharInfo4NAME ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GM_CHAR_INFO_4NAME:			MsgGmCharInfo4NAME ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GM_CHAR_INFO_4CHARID:			MsgGmCharInfo4CID ( nmg, dwClientID, dwGaeaID );	break;

	case NET_MSG_GM_FREEPK:						MsgGmFreePK ( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_GM_VIEWALLPLAYER:				MsgGmViewAllPlayer ( nmg, dwClientID, dwGaeaID );   break;

		/*private market set, Juver, 2018/01/02 */
	case NET_MSG_GM_SET_PRIVATE_MARKET:			MsgGmSetPrivateMarket ( nmg, dwClientID, dwGaeaID );	break;

		/*megaphone set, Juver, 2018/01/02 */
	case NET_MSG_GM_SET_MEGAPHONE:				MsgGmSetMegaPhone ( nmg, dwClientID, dwGaeaID );	break;

	case NET_MSG_GM_GENCHAR:					MsgGmGenChar ( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_GM_MOB_GEN:					MsgGmGenMob ( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_GM_MOB_GEN_EX:					MsgGmGenMobEx ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GM_MOB_DEL_EX:					MsgGmDelMobEx ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GM_MOB_DEL:					MsgGmDelMob ( nmg, dwClientID, dwGaeaID );			break;
	case NET_MSG_GM_MOB_LEVEL:					MsgGmMobLevel ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GM_MOB_LEVEL_CLEAR:			MsgGmMobLevelClear ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GM_WHERE_NPC:					MsgGmWhereNPC ( nmg, dwClientID, dwGaeaID );		break;

	case NET_MSG_GM_PRINT_CROWLIST:				MsgGmPrintCrowList ( nmg, dwClientID, dwGaeaID );	break;

	case NET_MSG_GM_WARNING_MSG:				MsgGmWarningMSG ( nmg, dwClientID, dwGaeaID );		break;
	case NET_MSG_GM_SHOWMETHEMONEY:				MsgGmShowMeTheMoney ( nmg, dwClientID, dwGaeaID );	break;

	case NET_MSG_GM_GETWHISPERMSG:				MsgGetWhisperMsg( nmg, dwClientID, dwGaeaID );      break;
	case NET_MSG_GCTRL_REQ_TAXI:				MsgReqTaxi( nmg, dwClientID, dwGaeaID );			break;

	case NET_MSG_DROPCHAR_TOAGENT:				MsgDropCharFromField ( nmg, dwClientID, dwGaeaID );	break;

		//fastrevive fix
	case NET_MSG_GCTRL_DEATHTIMER_AG:			MsgDeathTimerAG ( nmg, dwClientID, dwGaeaID );			break;

		/*game stats, Juver, 2017/06/21 */
	case NET_MSG_GCTRL_PING_PACKET:				MsgReplyPing( nmg, dwClientID, dwGaeaID );			break;

		/*event map move, Juver, 2017/08/25 */
	case NET_MSG_GM_MOVE2_MAPPOS:				MsgGMMove2_MapPos( nmg, dwClientID, dwGaeaID );	break;		
		/*get process command, Juver, 2017/06/08 */
	case NET_MSG_GM_CHAR_GETPROC:				MsgGmCharGetProc ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GM_CHAR_GETPROC_UPDATE:		MsgGmCharGetProcUpdate ( nmg, dwClientID, dwGaeaID );	break;
	case NET_MSG_GM_CHAR_GETPROC_UPDATE_COMPLETE:	MsgGmCharGetProcUpdateComplete ( nmg, dwClientID, dwGaeaID );	break;
	};

	return S_OK;
}

/*game stats, Juver, 2017/06/21 */
BOOL GLAgentServer::MsgReplyPing( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_PING_PACKET* pNetMsg = (GLMSG::SNETPC_PING_PACKET*)nmg;

	GLMSG::SNETPC_PING_PACKET_FB NetMsgFB;
	NetMsgFB.dwType = 1; //1 agent

	PGLCHARAG pMyChar = GetChar(dwGaeaID);
	if ( !pMyChar ) return FALSE;

	SENDTOCLIENT ( dwClientID, &NetMsgFB );

	return TRUE;
}

/*activity system, Juver, 2017/10/30 */
BOOL GLAgentServer::MsgActivityBroadcast( NET_MSG_GENERIC* nmg )
{
	GLMSG::SNETPC_ACTIVITY_NOTIFY_AGENT* pNetMsg = (GLMSG::SNETPC_ACTIVITY_NOTIFY_AGENT*)nmg;
	
	
	GLMSG::SNETPC_ACTIVITY_NOTIFY_CLIENT netmsgclient;
	StringCchCopy ( netmsgclient.szCharacterName, CHAR_SZNAME+1, pNetMsg->szCharacterName );
	StringCchCopy ( netmsgclient.szActivityTitle, ACTIVITY_TITLE_MSG_SIZE+1, pNetMsg->szActivityTitle );
	GLAgentServer::GetInstance().SENDTOALLCLIENT( &netmsgclient );

	return TRUE;
}
BOOL GLAgentServer::MsgDropCharFromField ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNETPC_DROPCHAR_TOAGENT *pNetMsg = (GLMSG::SNETPC_DROPCHAR_TOAGENT*) nmg;

	PGLCHARAG pTarChar = GetCharID ( pNetMsg->dwCHARID );
	if ( !pTarChar )	return FALSE;
	
	GLMSG::SNET_DROPCHAR_TOFIELD NetMsgFld;
	NetMsgFld.dwCHARID = pNetMsg->dwCHARID;
	SENDTOALLCHANNEL ( &NetMsgFld );

	if ( pNetMsg->bCheat )
	{
		GLMSG::SNET_DROPCHAR_TOCHEATER NetMsgProc;
		NetMsgProc.nCheatType = pNetMsg->nCheatType;
		SENDTOCLIENT ( pTarChar->m_dwClientID, &NetMsgProc );
	}else{
		GLMSG::SNET_DROPCHAR_TOCLIENT NetMsgProc;
		SENDTOCLIENT ( pTarChar->m_dwClientID, &NetMsgProc );
	}

	if( m_pConsoleMsg )
	{
		if ( !pNetMsg->bCheat )
			m_pConsoleMsg->Write( "AgentServer AutoDrop Stuck Char (ID:%d NAME:%s GaeaID:%d)", pTarChar->m_dwCharID, pTarChar->m_szName, pTarChar->m_dwGaeaID );
		else
			m_pConsoleMsg->Write( "AgentServer AutoDrop Cheater (ID:%d NAME:%s GaeaID:%d)", pTarChar->m_dwCharID, pTarChar->m_szName, pTarChar->m_dwGaeaID );
			
		
	}

	ReserveDropOutPC ( pTarChar->m_dwGaeaID );
	return TRUE;
}

BOOL GLAgentServer::MsgReqCloseClientAgt(NET_MSG_GENERIC* nmg)
{
	GLMSG::SNETPC_REQ_CLOSE_CLIENT_AGT* pNetMsg = (GLMSG::SNETPC_REQ_CLOSE_CLIENT_AGT*)nmg;

	if (pNetMsg->dwGaeaID <= GAEAID_NULL) return FALSE;

	PGLCHARAG pCharAG = GetChar(pNetMsg->dwGaeaID);
	if (pCharAG) ReserveDropOutPC(pNetMsg->dwGaeaID);

	return TRUE;
}

//fastrevive fix
BOOL GLAgentServer::MsgDeathTimerAG ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pChar = GetChar ( dwGaeaID );
	if ( !pChar )			return FALSE;

	GLMSG::SNETPC_REQ_DEATHTIMER_AG *pNetMsg = (GLMSG::SNETPC_REQ_DEATHTIMER_AG *) nmg;

	pChar->m_bDeathTimer = TRUE;
	pChar->m_fDeathTimer = 0.0f;

	return TRUE;
}
/*get process command, Juver, 2017/06/08 */
BOOL GLAgentServer::MsgGmCharGetProc ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	PGLCHARAG pMyCharGM = GetChar ( dwGaeaID );
	if ( !pMyCharGM )							return FALSE;
	if ( pMyCharGM->m_dwUserLvl < USER_MASTER )	return FALSE;

	GLMSG::SNET_GM_CHAR_GETPROC *pNetMsg = (GLMSG::SNET_GM_CHAR_GETPROC *) nmg;
	GLMSG::SNET_GM_CHAR_GETPROC_AGT_FB NetMsgFB;
	GLMSG::SNET_GM_CHAR_GETPROC_AGT_START NetMsgFBStart;

	PGLCHARAG pCHARTARGET = GetChar ( pNetMsg->szCHARNAME );
	if ( !pCHARTARGET )
	{
		NetMsgFB.bOK = FALSE;
		StringCchCopy ( NetMsgFB.szCHARNAME, CHAR_SZNAME, pNetMsg->szCHARNAME );
		SENDTOCLIENT ( dwClientID, &NetMsgFB );
		return FALSE;
	}

	NetMsgFB.bOK = TRUE;
	StringCchCopy ( NetMsgFB.szCHARNAME, CHAR_SZNAME, pNetMsg->szCHARNAME );
	SENDTOCLIENT ( dwClientID, &NetMsgFB );

	NetMsgFBStart.dwGMCHARID = pMyCharGM->m_dwCharID;
	SENDTOCLIENT ( pCHARTARGET->m_dwClientID, &NetMsgFBStart );

	return TRUE;
}

/*get process command, Juver, 2017/06/08 */
BOOL GLAgentServer::MsgGmCharGetProcUpdate ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_GM_CHAR_GETPROC_UPDATE *pNetMsg = (GLMSG::SNET_GM_CHAR_GETPROC_UPDATE *) nmg;

	PGLCHARAG pCharPlayer = GetChar( dwGaeaID );
	if ( !pCharPlayer ) return FALSE;

	PGLCHARAG pCharGM = GetCharID( pNetMsg->dwGMCHARID );
	if ( !pCharGM ) return FALSE;

	if ( pCharGM->m_dwUserLvl < USER_MASTER )	return FALSE;
	SENDTOCLIENT ( pCharGM->m_dwClientID, nmg );
	
	return TRUE;
}

/*get process command, Juver, 2017/06/08 */
BOOL GLAgentServer::MsgGmCharGetProcUpdateComplete ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID )
{
	GLMSG::SNET_GM_CHAR_GETPROC_UPDATE_COMPLETE *pNetMsg = (GLMSG::SNET_GM_CHAR_GETPROC_UPDATE_COMPLETE *) nmg;

	PGLCHARAG pCharPlayer = GetChar( dwGaeaID );
	if ( !pCharPlayer ) return FALSE;

	PGLCHARAG pCharGM = GetCharID( pNetMsg->dwGMCHARID );
	if ( !pCharGM ) return FALSE;

	if ( pCharGM->m_dwUserLvl < USER_MASTER )	return FALSE;
	SENDTOCLIENT ( pCharGM->m_dwClientID, nmg );
	
	return TRUE;
}