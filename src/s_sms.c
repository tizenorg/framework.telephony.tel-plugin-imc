/*
 * tel-plugin-imc
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Madhavi Akella <madhavi.a@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <glib.h>

#include <tcore.h>
#include <hal.h>
#include <core_object.h>
#include <plugin.h>
#include <queue.h>
#include <co_sms.h>
#include <user_request.h>
#include <storage.h>
#include <server.h>
#include <at.h>

#include "common/TelErr.h"
#include "s_common.h"
#include "s_sms.h"

/*=============================================================
							GSM-SMS Size
==============================================================*/
#define MAX_GSM_SMS_TPDU_SIZE						244
#define MAX_GSM_SMS_MSG_NUM							255
#define MAX_GSM_SMS_SERVICE_CENTER_ADDR				12		/* Maximum number of bytes of service center address */
#define MAX_GSM_SMS_CBMI_LIST_SIZE					100		/* Maximum number of CBMI list size for CBS 30*2=60  */
#define MAX_GSM_SMS_PARAM_RECORD_SIZE				156		/* Maximum number of bytes SMSP Record size (Y + 28), y : 0 ~ 128 */
#define MAX_GSM_SMS_STATUS_FILE_SIZE					2		/* Last Used TP-MR + SMS "Memory Cap. Exceeded" Noti Flag */
#define TAPI_SIM_SMSP_ADDRESS_LEN					20

/*=============================================================
							Device Ready
==============================================================*/
#define AT_SMS_DEVICE_READY			12		/* AT device ready */
#define SMS_DEVICE_READY				1		/* Telephony device ready */
#define SMS_DEVICE_NOT_READY			0		/* Telephony device not ready */

/*=============================================================
							CBMI Selection
==============================================================*/
#define SMS_CBMI_SELECTED_SOME		0x02	/* Some CBMIs are selected */
#define SMS_CBMI_SELECTED_ALL 			0x01	/* All CBMIs are selected */

/*=============================================================
							Message Status
==============================================================*/
#define AT_REC_UNREAD 					0		/* Received and Unread */
#define AT_REC_READ 					1		/* Received and Read */
#define AT_STO_UNSENT 					2		/* Unsent */
#define AT_STO_SENT 					3		/* Sent */
#define AT_ALL 							4		/* Unknown */

/*=============================================================
							Memory Status
==============================================================*/
#define AT_MEMORY_AVAILABLE 			0		/* Memory Available */
#define AT_MEMORY_FULL 				1		/* Memory Full */

/*=============================================================
		SIM CRSM SW1 and Sw2 Error definitions */

#define AT_SW1_SUCCESS 0x90
#define AT_SW2_SUCCESS 0
#define AT_SW1_LEN_RESP 0x9F

/*=============================================================*/


/*=========================================================
							Security
==============================================================*/
#define MAX_SEC_PIN_LEN							8
#define MAX_SEC_PUK_LEN							8
#define MAX_SEC_PHONE_LOCK_PW_LEN				39		/* Maximum Phone Locking Password Length */
#define MAX_SEC_SIM_DATA_STRING					256		/* Maximum Length of the DATA or RESPONSE. Restricted SIM Access, Generic SIM Access Message */
#define MAX_SEC_NUM_LOCK_TYPE						8		/* Maximum number of Lock Type used in Lock Information Message */
#define MAX_SEC_IMS_AUTH_LEN						512		/* Maximum Length of IMS Authentication Message */

/*=============================================================
							String Preprocessor
==============================================================*/
#define CR		'\r'		/* Carriage Return */

/*=============================================================
							Developer
==============================================================*/
#define TAPI_CODE_SUBJECT_TO_CHANGE				/* For folding future features */

#define SMS_SWAPBYTES16(x) \
{ \
    unsigned short int data = *(unsigned short int*)&(x); \
    data = ((data & 0xff00) >> 8) |    \
           ((data & 0x00ff) << 8);     \
    *(unsigned short int*)&(x) = data ;      \
}

void print_glib_list_elem(gpointer data, gpointer user_data);

gboolean util_byte_to_hex(const char *byte_pdu, char *hex_pdu, int num_bytes);

/* gaurav.kalra: For test */
void print_glib_list_elem(gpointer data, gpointer user_data)
{
	char *item = (char *)data;
	dbg("item: [%s]", item);
}

/*=============================================================
							Send Callback
==============================================================*/
static void on_confirmation_sms_message_send(TcorePending *p, gboolean result, void *user_data)
{
	dbg("Entered Function. Request message out from queue");

	dbg("TcorePending: [%p]", p);
	dbg("result: [%02x]", result);
	dbg("user_data: [%p]", user_data);

	if(result == TRUE)
	{
		dbg("SEND OK");
	}
	else /* Failed */
	{
		dbg("SEND NOK");
	}

	dbg("Exiting Function. Nothing to return");
}

/*=============================================================
							Utilities
==============================================================*/
static void util_sms_free_memory(void *sms_ptr)
{
	dbg("Entry");

	if(NULL != sms_ptr)
	{
		dbg("Freeing memory location: [%p]", sms_ptr);
		free(sms_ptr);
		sms_ptr = NULL;
	}
	else
	{
		err("Invalid memory location. Nothing to do.");
	}

	dbg("Exit");
}

#if 0
static void util_sms_get_length_of_sca(int* nScLength) {
	if (*nScLength % 2) {
		*nScLength = (*nScLength / 2) + 1;
	} else {
		*nScLength = *nScLength / 2;
	}

	return;
}

static int util_sms_decode_smsParameters(unsigned char *incoming, unsigned int length, struct telephony_sms_Params *params)
{
	int alpha_id_len = 0;
	int i = 0;
	int nOffset = 0;

	dbg(" RecordLen = %d", length);

	if(incoming == NULL || params == NULL)
		return FALSE;

	alpha_id_len = length -SMS_SMSP_PARAMS_MAX_LEN;

	if (alpha_id_len > 0)
	{
		if(alpha_id_len > SMS_SMSP_ALPHA_ID_LEN_MAX)
		{
			alpha_id_len = SMS_SMSP_ALPHA_ID_LEN_MAX;
		}

		for(i=0 ; i < alpha_id_len ; i++)
		{
			if(0xff == incoming[i])
			{
				dbg(" found");
				break;
			}
		}

		memcpy(params->szAlphaId, incoming, i);

		params->alphaIdLen = i;

		dbg(" Alpha id length = %d", i);

	}
	else
	{
		params->alphaIdLen = 0;
		dbg(" Alpha id length is zero");
	}

	//dongil01.park - start parse from here.
	params->paramIndicator = incoming[alpha_id_len];

	dbg(" Param Indicator = %02x", params->paramIndicator);

	//dongil01.park(2008/12/26) - DestAddr
	if((params->paramIndicator & SMSPValidDestAddr) == 0)
	{
		nOffset = nDestAddrOffset;

		if(0x00 == incoming[alpha_id_len + nOffset] || 0xff == incoming[alpha_id_len + nOffset])
		{
			params->tpDestAddr.dialNumLen = 0;

			dbg("DestAddr Length is 0");
		}
		else
		{
			if (0 < (int)incoming[alpha_id_len + nOffset])
			{
				params->tpDestAddr.dialNumLen = (int)(incoming[alpha_id_len + nOffset] - 1);

			        if(params->tpDestAddr.dialNumLen > SMS_SMSP_ADDRESS_LEN)
				        params->tpDestAddr.dialNumLen = SMS_SMSP_ADDRESS_LEN;
			}
			else
			{
				params->tpDestAddr.dialNumLen = 0;
			}

			params->tpDestAddr.numPlanId= incoming[alpha_id_len + (++nOffset)] & 0x0f ;
			params->tpDestAddr.typeOfNum= (incoming[alpha_id_len + nOffset] & 0x70)>>4 ;

			memcpy(params->tpDestAddr.diallingNum, &incoming[alpha_id_len + (++nOffset)], (params->tpDestAddr.dialNumLen)) ;

			dbg("Dest TON is %d",params->tpDestAddr.typeOfNum);
			dbg("Dest NPI is %d",params->tpDestAddr.numPlanId);
			dbg("Dest Length = %d",params->tpDestAddr.dialNumLen);
			dbg("Dest Addr = %s",params->tpDestAddr.diallingNum);

		}
	}

	//dongil01.park(2008/12/26) - SvcAddr
	if((params->paramIndicator & SMSPValidSvcAddr) == 0)
	{
		nOffset = nSCAAddrOffset;

		if(0x00 == (int)incoming[alpha_id_len + nOffset] || 0xff == (int)incoming[alpha_id_len + nOffset])
		{
			params->tpSvcCntrAddr.dialNumLen = 0;

			dbg(" SCAddr Length is 0");
		}
		else
		{
			if (0 < (int)incoming[alpha_id_len + nOffset] )
			{
				params->tpSvcCntrAddr.dialNumLen = (int)(incoming[alpha_id_len + nOffset] - 1);

			        if(params->tpSvcCntrAddr.dialNumLen > SMS_SMSP_ADDRESS_LEN)
				        params->tpSvcCntrAddr.dialNumLen = SMS_SMSP_ADDRESS_LEN;

				params->tpSvcCntrAddr.numPlanId= incoming[alpha_id_len + (++nOffset)] & 0x0f ;
				params->tpSvcCntrAddr.typeOfNum= (incoming[alpha_id_len + nOffset] & 0x70) >>4 ;

				memcpy(params->tpSvcCntrAddr.diallingNum, &incoming[alpha_id_len + (++nOffset)], (params->tpSvcCntrAddr.dialNumLen));

				dbg("SCAddr Length = %d ",params->tpSvcCntrAddr.dialNumLen);
				dbg("SCAddr TON is %d",params->tpSvcCntrAddr.typeOfNum);
				dbg("SCAddr NPI is %d",params->tpSvcCntrAddr.numPlanId);

				for(i = 0 ; i < (int)params->tpSvcCntrAddr.dialNumLen ; i ++)
					dbg("SCAddr = %d [%02x]",i,params->tpSvcCntrAddr.diallingNum[i]);
			}
			else
			{
				params->tpSvcCntrAddr.dialNumLen = 0;
			}
		}
	}
	else if ((0x00 < (int)incoming[alpha_id_len +nSCAAddrOffset] && (int)incoming[alpha_id_len +nSCAAddrOffset] <= 12)
			|| 0xff != (int)incoming[alpha_id_len +nSCAAddrOffset])
	{
		nOffset = nSCAAddrOffset;

		if(0x00 == (int)incoming[alpha_id_len + nOffset] || 0xff == (int)incoming[alpha_id_len + nOffset])
		{
			params->tpSvcCntrAddr.dialNumLen = 0;
			dbg("SCAddr Length is 0");
		}
		else
		{

			if (0 < (int)incoming[alpha_id_len + nOffset] )
			{
				params->tpSvcCntrAddr.dialNumLen = (int)(incoming[alpha_id_len + nOffset] - 1);

				params->tpSvcCntrAddr.dialNumLen = incoming[alpha_id_len + nOffset] -1;

			        if(params->tpSvcCntrAddr.dialNumLen > SMS_SMSP_ADDRESS_LEN)
				        params->tpSvcCntrAddr.dialNumLen = SMS_SMSP_ADDRESS_LEN;

				params->tpSvcCntrAddr.numPlanId= incoming[alpha_id_len + (++nOffset)] & 0x0f ;
				params->tpSvcCntrAddr.typeOfNum= (incoming[alpha_id_len + nOffset] & 0x70) >>4 ;

				memcpy(params->tpSvcCntrAddr.diallingNum, &incoming[alpha_id_len + (++nOffset)],
						(params->tpSvcCntrAddr.dialNumLen)) ;

				dbg("SCAddr Length = %d ",params->tpSvcCntrAddr.dialNumLen);
				dbg("SCAddr TON is %d",params->tpSvcCntrAddr.typeOfNum);
				dbg("SCAddr NPI is %d",params->tpSvcCntrAddr.numPlanId);

				for(i = 0 ; i < (int)params->tpSvcCntrAddr.dialNumLen ; i ++)
					dbg("SCAddr = %d [%02x]",i,params->tpSvcCntrAddr.diallingNum[i]);
			}
			else
			{
				params->tpSvcCntrAddr.dialNumLen = 0;
			}
		}

	}

	if((params->paramIndicator & SMSPValidPID) == 0 &&	(alpha_id_len + nPIDOffset) < MAX_GSM_SMS_PARAM_RECORD_SIZE)
	{
		params->tpProtocolId = incoming[alpha_id_len + nPIDOffset];
	}
	if((params->paramIndicator & SMSPValidDCS) == 0 && (alpha_id_len + nDCSOffset) < MAX_GSM_SMS_PARAM_RECORD_SIZE)
	{
		params->tpDataCodingScheme = incoming[alpha_id_len + nDCSOffset];
	}
	if((params->paramIndicator & SMSPValidVP) == 0 && (alpha_id_len + nVPOffset) < MAX_GSM_SMS_PARAM_RECORD_SIZE)
	{
		params->tpValidityPeriod = incoming[alpha_id_len + nVPOffset];
	}

	dbg(" Alpha Id(Len) = %d",(int)params->alphaIdLen);

	for (i=0; i< (int)params->alphaIdLen ; i++)
	{
		dbg(" Alpha Id = [%d] [%c]",i,params->szAlphaId[i]);
	}
	dbg(" PID = %d",params->tpProtocolId);
	dbg(" DCS = %d",params->tpDataCodingScheme);
	dbg(" VP = %d",params->tpValidityPeriod);

	return TRUE;
}
#endif

/*=============================================================
							Notifications
==============================================================*/
static gboolean on_event_sms_ready_status(CoreObject *o, const void *event_info, void *user_data)
{
	struct tnoti_sms_ready_status readyStatusInfo = {0,};
	char *line = NULL;
	GSList* tokens = NULL;
	GSList* lines = NULL;
	char *pResp = NULL;
	//CoreObject *o = NULL;

	int rtn = -1 , status = 0;

	dbg(" Func Entrance");

	lines = (GSList *)event_info;
	if (1 != g_slist_length(lines))
	{
		dbg("unsolicited msg but multiple line");
		goto OUT;
	}
	line = (char *)(lines->data);

	dbg(" Func Entrance");

	if(line!=NULL)
	{
		dbg("Response OK");
			dbg("noti line is %s", line);
			tokens = tcore_at_tok_new(line);
			pResp = g_slist_nth_data(tokens, 0);
			if (pResp !=NULL)
				status = atoi(pResp);

	}
	else
	{
		dbg("Response NOK");
	}

	if (status == AT_SMS_DEVICE_READY)
	{
		readyStatusInfo.status = SMS_DEVICE_READY;
		tcore_sms_set_ready_status(o, readyStatusInfo.status);
		dbg("SMS Ready status = [%s]", readyStatusInfo.status ? "TRUE" : "FALSE");
		rtn = tcore_server_send_notification(tcore_plugin_ref_server(tcore_object_ref_plugin(o)), o, TNOTI_SMS_DEVICE_READY, sizeof(struct tnoti_sms_ready_status), &readyStatusInfo);
		dbg(" Return value [%d]",rtn);
	}
	else
	{
		readyStatusInfo.status = SMS_DEVICE_NOT_READY;
	}

OUT:
	if(NULL!=tokens)
		tcore_at_tok_free(tokens);
	return TRUE;
}

static gboolean on_event_sms_incom_msg(CoreObject *o, const void *event_info, void *user_data)
{
	int rtn = -1;
	GSList *tokens = NULL;
	GSList *lines = NULL;
	char *line = NULL;
	int length = 0;
	unsigned char *bytePDU = NULL;
	struct tnoti_sms_umts_msg gsmMsgInfo;

	dbg("Entered Function");

	lines = (GSList *)event_info;
	memset(&gsmMsgInfo, 0x00, sizeof(struct tnoti_sms_umts_msg));

	if(2 != g_slist_length(lines))
	{
		err("Invalid number of lines for +CMT. Must be 2");
		return FALSE;
	}

	line = (char *)g_slist_nth_data(lines, 0); /* Fetch Line 1 */

	dbg("Line 1: [%s]", line);

	if (!line)
	{
		err("Line 1 is invalid");
		return FALSE;
	}

	tokens = tcore_at_tok_new(line); /* Split Line 1 into tokens */

	dbg("Alpha ID: [%02x]", g_slist_nth_data(tokens, 0)); /* 0: Alpha ID */

	length = atoi((char *)g_slist_nth_data(tokens, 1));

	dbg("Length: [%d]", length);	/* 1: PDU Length */

	gsmMsgInfo.msgInfo.msgLength = length;

	line = (char *)g_slist_nth_data(lines, 1); /* Fetch Line 2 */

	dbg("Line 2: [%s]", line);

	if (!line)
	{
		err("Line 2 is invalid");
		return FALSE;
	}

	/* Convert to Bytes */
	bytePDU = (unsigned char *)util_hexStringToBytes(line);

	if(NULL == bytePDU)
	{
		err("bytePDU is NULL");
		return FALSE;
	}

	memcpy(gsmMsgInfo.msgInfo.sca, bytePDU, (strlen(line)/2 - length));
	memcpy(gsmMsgInfo.msgInfo.tpduData, &bytePDU[(strlen(line)/2 - length)], length);

	util_hex_dump("      ", strlen(line)/2, bytePDU);
	util_hex_dump("      ", (strlen(line)/2 - length), gsmMsgInfo.msgInfo.sca);
	util_hex_dump("      ", length, gsmMsgInfo.msgInfo.tpduData);

	rtn = tcore_server_send_notification(tcore_plugin_ref_server(tcore_object_ref_plugin(o)), o, TNOTI_SMS_INCOM_MSG, sizeof(struct tnoti_sms_umts_msg), &gsmMsgInfo);

	return TRUE;
}

static gboolean on_event_sms_memory_status(CoreObject *o, const void *event_info, void *user_data)
{
	struct tnoti_sms_memory_status memStatusInfo = {0,};

	int rtn = -1 ,memoryStatus = -1;
	GSList *tokens=NULL;
	GSList *lines=NULL;
	char *line = NULL , *pResp = NULL;

	lines = (GSList *)event_info;
        if (1 != g_slist_length(lines))
        {
                dbg("unsolicited msg but multiple line");
        }

	line = (char*)(lines->data);


	dbg(" Func Entrance");

	if (line)
	{
		dbg("Response OK");
		tokens = tcore_at_tok_new(line);
		pResp = g_slist_nth_data(tokens, 0);

		if(pResp)
		{
			memoryStatus = atoi(pResp);
			dbg("memoryStatus is %d",memoryStatus);
		}

	}else
	{
		dbg("Response NOK");

	}

	if (memoryStatus == 0) //SIM Full condition
	{
		memStatusInfo.status = SMS_PHONE_MEMORY_STATUS_FULL;
	}


	dbg("memory status - %d",memStatusInfo.status);

	rtn = tcore_server_send_notification(tcore_plugin_ref_server(tcore_object_ref_plugin(o)), o, TNOTI_SMS_MEMORY_STATUS, sizeof(struct tnoti_sms_memory_status), &memStatusInfo);
	dbg(" Return value [%d]",rtn);
	return TRUE;

}

static gboolean on_event_sms_cb_incom_msg(CoreObject *o, const void *event_info, void *user_data)
{
	//+CBM: <length><CR><LF><pdu>

	struct tnoti_sms_cellBroadcast_msg cbMsgInfo;

	int rtn = -1 , length = 0;
	char * line = NULL, *pdu = NULL, *pResp = NULL;
	GSList *tokens = NULL;
	GSList *lines = NULL;

	dbg(" Func Entrance");

	lines = (GSList *)event_info;

	memset(&cbMsgInfo, 0, sizeof(struct tnoti_sms_cellBroadcast_msg));

	line = (char *)(lines->data);

	if (line != NULL)
	{
			dbg("Response OK");
			dbg("Noti line is %s",line);
			pResp = g_slist_nth_data(tokens, 0);
			if (pResp)
			{
				length = atoi(pResp);
			}else
			{
				dbg("token 0 is null");
			}
			pdu = g_slist_nth_data(tokens, 3);
			if (pdu != NULL)
			{
				cbMsgInfo.cbMsg.length = length;
				cbMsgInfo.cbMsg.cbMsgType = SMS_CB_MSG_CBS ; //TODO - Need to check for other CB types

				dbg("CB Msg LENGTH [%2x](((( %d ))))", length, cbMsgInfo.cbMsg.length);

				if ( (cbMsgInfo.cbMsg.length >0) && ((SMS_CB_PAGE_SIZE_MAX +1)  > cbMsgInfo.cbMsg.length))
				{
					memcpy(cbMsgInfo.cbMsg.msgData, (char*)pdu, cbMsgInfo.cbMsg.length);
					rtn = tcore_server_send_notification(tcore_plugin_ref_server(tcore_object_ref_plugin(o)), o, TNOTI_SMS_CB_INCOM_MSG, sizeof(struct tnoti_sms_cellBroadcast_msg), &cbMsgInfo);
				}
				else
				{
					dbg("Invalid Message Length");
				}

			}
			else
			{
				dbg("Recieved NULL pdu");
			}
	}
	else
	{
			dbg("Response NOK");
	}


	dbg(" Return value [%d]",rtn);

	return TRUE;
}

/*=============================================================
							Responses
==============================================================*/
static void on_response_sms_delete_msg_cnf(TcorePending *p, int data_len, const void *data, void *user_data)
{
	struct tresp_sms_delete_msg delMsgInfo = {0,};
	UserRequest *ur = NULL;
	const TcoreATResponse *atResp = data;

	int rtn = -1;
	int *index = (int *)user_data;

	dbg(" Func Entrance");

	ur = tcore_pending_ref_user_request(p);
	if (atResp->success)
	{
		dbg("Response OK");
		delMsgInfo.index = *index;
		delMsgInfo.result = SMS_SENDSMS_SUCCESS;

	}
	else
	{
		dbg("Response NOK");
		delMsgInfo.index = *index;
		delMsgInfo.result = SMS_DEVICE_FAILURE;

	}

	rtn = tcore_user_request_send_response(ur, TRESP_SMS_DELETE_MSG, sizeof(struct tresp_sms_delete_msg), &delMsgInfo);

	return;
}

static void on_response_sms_save_msg_cnf(TcorePending *p, int data_len, const void *data, void *user_data)
{
	struct tresp_sms_save_msg saveMsgInfo = {0,};
	UserRequest *ur = NULL;
	const TcoreATResponse *atResp = data;
	GSList *tokens = NULL;
	char *line = NULL;
	char *pResp = NULL;
	int rtn = -1;

	ur = tcore_pending_ref_user_request(p);
	if (atResp->success)
	{
		dbg("Response OK");
		if(atResp->lines)
		{
			line = (char *)atResp->lines->data;
			tokens = tcore_at_tok_new(line);
			pResp = g_slist_nth_data(tokens, 0);
			if (pResp)
			{
				dbg("0: %s", pResp);
		 		saveMsgInfo.index = (atoi(pResp) - 1); /* IMC index starts from 1 */
				saveMsgInfo.result = SMS_SENDSMS_SUCCESS;
		 	}
			else
			{
				dbg("No Tokens");
				saveMsgInfo.index = -1;
				saveMsgInfo.result = SMS_DEVICE_FAILURE;
			}

		}
	}
	else
	{
		dbg("Response NOK");
		saveMsgInfo.index = -1;
		saveMsgInfo.result = SMS_DEVICE_FAILURE;
	}

	rtn = tcore_user_request_send_response(ur, TRESP_SMS_SAVE_MSG, sizeof(struct tresp_sms_save_msg), &saveMsgInfo);
	dbg("Return value [%d]", rtn);
	return;
}

#if 0
static void on_response_sms_deliver_rpt_cnf(TcorePending *p, int data_len, const void *data, void *user_data)
{

	struct tresp_sms_set_delivery_report deliverReportInfo = {0,};
	UserRequest *ur = NULL;
	const TcoreATResponse *atResp = data;
	GSList *tokens=NULL;
	char *line = NULL , *pResp = NULL;
	int rtn = -1;

	dbg(" Func Entrance");


	if (atResp->success)
	{
		dbg("Response OK");
		if(atResp->lines)
		{
			line = (char*)atResp->lines->data;
			tokens = tcore_at_tok_new(line);
			 pResp = g_slist_nth_data(tokens, 0);
			 if (pResp)
		 	{
	 			deliverReportInfo.result = SMS_SENDSMS_SUCCESS;
		 	}
			else
			{
				dbg("No tokens");
				deliverReportInfo.result = SMS_DEVICE_FAILURE;
			}
		}else
		{
			dbg("No lines");
			deliverReportInfo.result = SMS_DEVICE_FAILURE;
		}
	}else
	{
		dbg("Response NOK");
	}


	rtn = tcore_user_request_send_response(ur, TRESP_SMS_SET_DELIVERY_REPORT, sizeof(struct tresp_sms_set_delivery_report), &deliverReportInfo);

	dbg(" Return value [%d]",rtn);

	return;

}
#endif

static void on_response_send_umts_msg(TcorePending *pending, int data_len, const void *data, void *user_data)
{
	const TcoreATResponse *at_response = data;
	struct tresp_sms_send_umts_msg resp_umts;
	UserRequest *user_req = NULL;

	int msg_ref = 0;
	GSList *tokens = NULL;
	char *gslist_line = NULL, *line_token = NULL;

	dbg("Entry");

	user_req = tcore_pending_ref_user_request(pending);

	if(NULL == user_req)
	{
		err("No user request");

		dbg("Exit");
		return;
	}

	memset(&resp_umts, 0x00, sizeof(resp_umts));

	if(at_response->success > 0) //success
	{
		dbg("Response OK");
		if(at_response->lines)	//lines present in at_response
		{
			gslist_line = (char *)at_response->lines->data;
			dbg("gslist_line: [%s]", gslist_line);

			tokens = tcore_at_tok_new(gslist_line); //extract tokens

			line_token = g_slist_nth_data(tokens, 0);
			if (line_token != NULL)
			{
				msg_ref = atoi(line_token);
				dbg("Message Reference: [%d]", msg_ref);

				resp_umts.result = SMS_SENDSMS_SUCCESS;
			}
			else
			{
				dbg("No Message Reference received");
				resp_umts.result = SMS_DEVICE_FAILURE;
			}
		}
		else //no lines in at_response
		{
			dbg("No lines");
			resp_umts.result = SMS_DEVICE_FAILURE;
		}
	}
	else //failure
	{
		dbg("Response NOK");
		resp_umts.result = SMS_DEVICE_FAILURE;
	}

	tcore_user_request_send_response(user_req, TRESP_SMS_SEND_UMTS_MSG, sizeof(resp_umts), &resp_umts);

	dbg("Exit");
	return;
}

static void on_response_read_msg(TcorePending *pending, int data_len, const void *data, void *user_data)
{
	const TcoreATResponse *at_response = data;
	struct tresp_sms_read_msg resp_read_msg;
	UserRequest *user_req = NULL;

	GSList *tokens=NULL;
	char *gslist_line = NULL, *line_token = NULL, *byte_pdu = NULL, *hex_pdu = NULL;
	int sca_length = 0;
	int msg_status = 0, alpha_id = 0, pdu_len = 0;
	int index = (int)(uintptr_t)user_data;

	dbg("Entry");
	dbg("index: [%d]", index);
	dbg("lines: [%p]", at_response->lines);
	g_slist_foreach(at_response->lines, print_glib_list_elem, NULL); //for debug log

	user_req = tcore_pending_ref_user_request(pending);
	if (NULL == user_req)
	{
		err("No user request");

		dbg("Exit");
		return;
	}

	memset(&resp_read_msg, 0x00, sizeof(resp_read_msg));

	if (at_response->success > 0)
	{
		dbg("Response OK");
		if (at_response->lines)
		{
			//fetch first line
			gslist_line = (char *)at_response->lines->data;

			dbg("gslist_line: [%s]", gslist_line);

			tokens = tcore_at_tok_new(gslist_line);
			dbg("Number of tokens: [%d]", g_slist_length(tokens));
			g_slist_foreach(tokens, print_glib_list_elem, NULL); /* gaurav.kalra: For test */

			line_token = g_slist_nth_data(tokens, 0); //First Token: Message Status
			if (line_token != NULL)
			{
				msg_status = atoi(line_token);
				dbg("msg_status is %d",msg_status);
				switch (msg_status)
				{
					case AT_REC_UNREAD:
						resp_read_msg.dataInfo.msgStatus = SMS_STATUS_UNREAD;
						break;

					case AT_REC_READ:
						resp_read_msg.dataInfo.msgStatus = SMS_STATUS_READ;
						break;

					case AT_STO_UNSENT:
						resp_read_msg.dataInfo.msgStatus = SMS_STATUS_UNSENT;
						break;

					case AT_STO_SENT:
						resp_read_msg.dataInfo.msgStatus = SMS_STATUS_SENT;
						break;

					case AT_ALL: //Fall Through
					default: //Fall Through
						resp_read_msg.dataInfo.msgStatus = SMS_STATUS_RESERVED;
						break;
				}
			}

			line_token = g_slist_nth_data(tokens, 1); //Second Token: AlphaID
			if (line_token != NULL)
			{
				alpha_id = atoi(line_token);
				dbg("AlphaID: [%d]", alpha_id);
			}

			line_token = g_slist_nth_data(tokens, 2); //Third Token: Length
			if (line_token != NULL)
			{
				pdu_len = atoi(line_token);
				dbg("Length: [%d]", pdu_len);
			}

			//fetch second line
			gslist_line = (char *)at_response->lines->next->data;

			dbg("gslist_line: [%s]", gslist_line);

			tokens = tcore_at_tok_new(gslist_line);
			dbg("Number of tokens: [%d]", g_slist_length(tokens));
			g_slist_foreach(tokens, print_glib_list_elem, NULL); //for debug log

			hex_pdu = g_slist_nth_data(tokens, 0); //Fetch SMS PDU
			if (NULL != hex_pdu)
			{
				util_hex_dump("    ", sizeof(hex_pdu), (void *)hex_pdu);

				byte_pdu = util_hexStringToBytes(hex_pdu);

				sca_length = (int)byte_pdu[0];

				resp_read_msg.dataInfo.simIndex = index; //Retrieving index stored as user_data

				if(0 == sca_length)
				{
					dbg("SCA Length is 0");

					resp_read_msg.dataInfo.smsData.msgLength =  pdu_len  - (sca_length+1);
					dbg("msgLength: [%d]", resp_read_msg.dataInfo.smsData.msgLength);

					if ((resp_read_msg.dataInfo.smsData.msgLength > 0)
						&& (resp_read_msg.dataInfo.smsData.msgLength <= 0xff))
					{
						memset(resp_read_msg.dataInfo.smsData.sca, 0, TAPI_SIM_SMSP_ADDRESS_LEN);
						memcpy(resp_read_msg.dataInfo.smsData.tpduData, &byte_pdu[2], resp_read_msg.dataInfo.smsData.msgLength);

						resp_read_msg.result = SMS_SUCCESS;
					}
					else
					{
						err("Invalid Message Length");

						resp_read_msg.result = SMS_INVALID_PARAMETER_FORMAT;
					}
				}
				else
				{
					dbg("SCA Length is not 0");

					resp_read_msg.dataInfo.smsData.msgLength =  (pdu_len - (sca_length+1));
					dbg("msgLength: [%d]", resp_read_msg.dataInfo.smsData.msgLength);

					if ((resp_read_msg.dataInfo.smsData.msgLength > 0)
						&& (resp_read_msg.dataInfo.smsData.msgLength <= 0xff))
					{
						memcpy(resp_read_msg.dataInfo.smsData.sca, (char *)byte_pdu, (sca_length+1));
						memcpy(resp_read_msg.dataInfo.smsData.tpduData, &byte_pdu[sca_length+1], resp_read_msg.dataInfo.smsData.msgLength);

						util_hex_dump("    ", SMS_SMSP_ADDRESS_LEN, (void *)resp_read_msg.dataInfo.smsData.sca);
						util_hex_dump("    ", (SMS_SMDATA_SIZE_MAX + 1), (void *)resp_read_msg.dataInfo.smsData.tpduData);
						util_hex_dump("    ", sizeof(byte_pdu), (void *)byte_pdu);

						resp_read_msg.result = SMS_SUCCESS;
					}
					else
					{
						err("Invalid Message Length");

						resp_read_msg.result = SMS_INVALID_PARAMETER_FORMAT;
					}
				}
			}
			else
			{
				dbg("NULL PDU");
				resp_read_msg.result = SMS_PHONE_FAILURE;
			}
		}
		else
		{
			dbg("No lines");
			resp_read_msg.result = SMS_PHONE_FAILURE;
		}
	}
	else
	{
		err("Response NOK");
		resp_read_msg.result = SMS_PHONE_FAILURE;
	}

	tcore_user_request_send_response(user_req, TRESP_SMS_READ_MSG, sizeof(resp_read_msg), &resp_read_msg);

	dbg("Exit");
	return;
}

static void on_response_get_msg_indices(TcorePending *pending, int data_len, const void *data, void *user_data)
{
	const TcoreATResponse *at_response = data;
	struct tresp_sms_get_storedMsgCnt resp_stored_msg_cnt;
	UserRequest *user_req = NULL;
	struct tresp_sms_get_storedMsgCnt *resp_stored_msg_cnt_prev = NULL;

	GSList *tokens = NULL;
	char *gslist_line = NULL, *line_token = NULL;
	int gslist_line_count = 0, ctr_loop = 0;

	dbg("Entry");

	resp_stored_msg_cnt_prev = (struct tresp_sms_get_storedMsgCnt *)user_data;
	user_req = tcore_pending_ref_user_request(pending);

	memset(&resp_stored_msg_cnt, 0x00, sizeof(resp_stored_msg_cnt));

	if (at_response->success)
	{
		dbg("Response OK");
		if(at_response->lines)
		{
			gslist_line_count = g_slist_length(at_response->lines);

			if (gslist_line_count > SMS_GSM_SMS_MSG_NUM_MAX)
				gslist_line_count = SMS_GSM_SMS_MSG_NUM_MAX;

			dbg("Number of lines: [%d]", gslist_line_count);
			g_slist_foreach(at_response->lines, print_glib_list_elem, NULL); //for debug log

			for (ctr_loop = 0; ctr_loop < gslist_line_count ; ctr_loop++)
     			{
				gslist_line = (char *)g_slist_nth_data(at_response->lines, ctr_loop); /* Fetch Line i */

				dbg("gslist_line [%d] is [%s]", ctr_loop, gslist_line);

				if (NULL != gslist_line)
				{
					tokens = tcore_at_tok_new(gslist_line);

					g_slist_foreach(tokens, print_glib_list_elem, NULL); /* gaurav.kalra: For test */

					line_token = g_slist_nth_data(tokens, 0);
					if (NULL != line_token)
					{
						resp_stored_msg_cnt.storedMsgCnt.indexList[ctr_loop] = atoi(line_token);
					}
					else
					{
						resp_stored_msg_cnt.result = SMS_DEVICE_FAILURE;
						dbg("line_token of gslist_line [%d] is NULL", ctr_loop);

						break;
					}
				}
				else
				{
					resp_stored_msg_cnt.result = SMS_DEVICE_FAILURE;
					dbg("gslist_line [%d] is NULL", ctr_loop);

					break;
				}
     			}
		}
		else
		{
			dbg("No lines.");
			if(resp_stored_msg_cnt_prev->storedMsgCnt.usedCount == 0) //Check if used count is zero
			{
				resp_stored_msg_cnt.result = SMS_SENDSMS_SUCCESS;
			}
			else
			{
				resp_stored_msg_cnt.result = SMS_DEVICE_FAILURE;
			}
		}
	}
	else
	{
		dbg("Respnose NOK");
		resp_stored_msg_cnt.result = SMS_DEVICE_FAILURE;
	}

	resp_stored_msg_cnt.storedMsgCnt.totalCount = resp_stored_msg_cnt_prev->storedMsgCnt.totalCount;
	resp_stored_msg_cnt.storedMsgCnt.usedCount = resp_stored_msg_cnt_prev->storedMsgCnt.usedCount;

	util_sms_free_memory(resp_stored_msg_cnt_prev);

	dbg("total: [%d]", resp_stored_msg_cnt.storedMsgCnt.totalCount);
	dbg("used: [%d]", resp_stored_msg_cnt.storedMsgCnt.usedCount);
	dbg("result: [%d]", resp_stored_msg_cnt.result);
	for(ctr_loop = 0; ctr_loop < gslist_line_count; ctr_loop++)
	{
		dbg("index: [%d]", resp_stored_msg_cnt.storedMsgCnt.indexList[ctr_loop]);
	}

	tcore_user_request_send_response(user_req, TRESP_SMS_GET_STORED_MSG_COUNT, sizeof(resp_stored_msg_cnt), &resp_stored_msg_cnt);

	dbg("Exit");
	return;
}

static void on_response_get_storedMsgCnt(TcorePending *pending, int data_len, const void *data, void *user_data)
{
	UserRequest *ur = NULL, *ur_dup = NULL;
	struct tresp_sms_get_storedMsgCnt *respStoredMsgCnt = NULL;
	const TcoreATResponse *atResp = data;
	GSList *tokens=NULL;
	char *line = NULL , *pResp = NULL , *cmd_str = NULL;
	TcoreATRequest *atReq = NULL;
	int usedCnt = 0, totalCnt = 0, result = 0;

	TcorePending *pending_new = NULL;
	CoreObject *o = NULL;

	dbg("Entered");

	respStoredMsgCnt = malloc(sizeof(struct tresp_sms_get_storedMsgCnt));

	ur = tcore_pending_ref_user_request(pending);
	ur_dup = tcore_user_request_ref(ur);
	o = tcore_pending_ref_core_object(pending);

	if (atResp->success > 0)
	{
		dbg("Response OK");
		if(NULL != atResp->lines)
		{
			line = (char *)atResp->lines->data;
			dbg("line is %s",line);

			tokens = tcore_at_tok_new(line);
			pResp = g_slist_nth_data(tokens, 0);

			if (pResp)
		 	{
		 		usedCnt =atoi(pResp);
				dbg("used cnt is %d",usedCnt);
			}

			pResp = g_slist_nth_data(tokens, 1);
			if (pResp)
		 	{
		 		totalCnt =atoi(pResp);
				result = SMS_SENDSMS_SUCCESS;

				respStoredMsgCnt->storedMsgCnt.usedCount = usedCnt;
				respStoredMsgCnt->storedMsgCnt.totalCount = totalCnt;
				respStoredMsgCnt->result = result;

				dbg("used %d, total %d, result %d",usedCnt, totalCnt,result);

				pending_new = tcore_pending_new(o, 0);
				cmd_str = g_strdup_printf("AT+CMGL=4\r");
				atReq = tcore_at_request_new((const char *)cmd_str, "+CMGL:", TCORE_AT_MULTILINE);

				dbg("cmd str is %s",cmd_str);

				tcore_pending_set_request_data(pending_new, 0,atReq);
				tcore_pending_set_response_callback(pending_new, on_response_get_msg_indices, (void *)respStoredMsgCnt);
				tcore_pending_link_user_request(pending_new, ur_dup);
				tcore_pending_set_send_callback(pending_new, on_confirmation_sms_message_send, NULL);
				tcore_hal_send_request(tcore_object_get_hal(o), pending_new);

				dbg("Exit");
				return;

			}
		}else
		{
				dbg("No data");
				result = SMS_DEVICE_FAILURE;
		}
	}

	err("Response NOK");

	dbg("Exit");
	return;
}

static void on_response_get_sca(TcorePending *pending, int data_len, const void *data, void *user_data)
{
	const TcoreATResponse *at_response = data;
	struct tresp_sms_get_sca respGetSca;
	UserRequest *user_req = NULL;

	GSList *tokens = NULL;
	char *gslist_line = NULL, *sca_addr = NULL, *sca_toa = NULL;

	dbg("Entry");

	memset(&respGetSca, 0, sizeof(respGetSca));

	user_req = tcore_pending_ref_user_request(pending);

	if (at_response->success)
	{
		dbg("Response OK");
		if(at_response->lines)
		{
			gslist_line = (char *)at_response->lines->data;

			tokens = tcore_at_tok_new(gslist_line);
			sca_addr = g_slist_nth_data(tokens, 0);
			sca_toa = g_slist_nth_data(tokens, 1);

			if ((NULL != sca_addr)
				&& (NULL != sca_toa))
			{
				dbg("sca_addr: [%s]. sca_toa: [%s]", sca_addr, sca_toa);

				respGetSca.scaAddress.dialNumLen = strlen(sca_addr);

				if(145 == atoi(sca_toa))
				{
					respGetSca.scaAddress.typeOfNum = SIM_TON_INTERNATIONAL;
				}
				else
				{
					respGetSca.scaAddress.typeOfNum = SIM_TON_NATIONAL;
				}

				respGetSca.scaAddress.numPlanId = 0;

				memcpy(respGetSca.scaAddress.diallingNum, sca_addr, strlen(sca_addr));

				dbg("len [%d], sca_addr [%s], TON [%d], NPI [%d]", respGetSca.scaAddress.dialNumLen, respGetSca.scaAddress.diallingNum, respGetSca.scaAddress.typeOfNum, respGetSca.scaAddress.numPlanId);

				respGetSca.result = SMS_SENDSMS_SUCCESS;
			}
			else
			{
				err("sca_addr OR sca_toa NULL");
				respGetSca.result = SMS_DEVICE_FAILURE;
			}
		}
		else
		{
			dbg("NO Lines");
			respGetSca.result = SMS_DEVICE_FAILURE;
		}
	}
	else
	{
		dbg("Response NOK");
		respGetSca.result = SMS_DEVICE_FAILURE;
	}

	tcore_user_request_send_response(user_req, TRESP_SMS_GET_SCA, sizeof(respGetSca), &respGetSca);

	dbg("Exit");
	return;
}

static void on_response_set_sca(TcorePending *pending, int data_len, const void *data, void *user_data)
{
	/*
	Response is expected in this format
	OK
		or
	+CMS ERROR: <err>
	*/

	//CoreObject *obj = user_data;
	UserRequest *ur;
	//copies the AT response data to resp
	const TcoreATResponse *atResp = data;
	struct tresp_sms_set_sca respSetSca;

	memset(&respSetSca, 0, sizeof(struct tresp_sms_set_sca));

	ur = tcore_pending_ref_user_request(pending);
	if (!ur)
	{
		dbg("no user_request");
		return;
	}

	if (atResp->success >0)
	{
		dbg("RESPONSE OK");
		respSetSca.result = SMS_SUCCESS;
	}
	else
	{
		dbg("RESPONSE NOK");
		respSetSca.result = SMS_DEVICE_FAILURE;
	}

	tcore_user_request_send_response(ur, TRESP_SMS_SET_SCA, sizeof(struct tresp_sms_set_sca), &respSetSca);

	return;
}

static void on_response_get_cb_config(TcorePending *p, int data_len, const void *data, void *user_data)
{
	UserRequest *ur;
	struct tresp_sms_get_cb_config respGetCbConfig;
	const TcoreATResponse *atResp = data;
	GSList *tokens=NULL;

	int i = 0, mode =0 , result = 0;
	char *mid = NULL, *pResp = NULL, *line = NULL, *res = NULL;
	char delim[] = ",";

	memset(&respGetCbConfig, 0, sizeof(struct tresp_sms_get_cb_config));

	ur = tcore_pending_ref_user_request(p);
	if (!ur)
	{
		dbg("no user_request");
		return;
	}

	if (atResp->success)
	{
		dbg("Response OK");
		if(atResp->lines)
		{
			line = (char*)atResp->lines->data;
			if (line != NULL)
			{
				dbg("line is %s",line);
				tokens = tcore_at_tok_new(line);
				pResp = g_slist_nth_data(tokens, 0);
				if (pResp)
				{
					mode = atoi(pResp);
					respGetCbConfig.cbConfig.bCBEnabled = mode;
					pResp = g_slist_nth_data(tokens, 1);
					if (pResp)
					{
						mid = strtok(pResp, delim); i = 0;
							while( res != NULL )
							{
						    		res = strtok( NULL, delim );
    						  		dbg("mid is %s%s\n", mid,res);
								if (res != NULL)
								{
									if (strlen(res) >0)
										{
						    					respGetCbConfig.cbConfig.msgIDs[i] = atoi(res);
						    					i++;
										}
								}
							}
					}
					else
					{
							result = SMS_DEVICE_FAILURE;
					}
					respGetCbConfig.cbConfig.msgIdCount = i;

				}
				else
				{
						result = SMS_DEVICE_FAILURE;
				}
				//dcs = g_slist_nth_data(tokens, 2); DCS not needed by telephony
			}
			else
			{
				dbg("line is NULL");
				result = SMS_DEVICE_FAILURE;
			}


		}
		else
		{
				result = SMS_DEVICE_FAILURE;
				dbg("atresp->lines is NULL");
		}
	}
	else
	{
			result = SMS_DEVICE_FAILURE;
			dbg("RESPONSE NOK");
	}

	// Todo max list count and selectedid

	tcore_user_request_send_response(ur, TRESP_SMS_GET_CB_CONFIG, sizeof(struct tresp_sms_get_cb_config), &respGetCbConfig);

	return;
}

static void on_response_set_cb_config(TcorePending *pending, int data_len, const void *data, void *user_data)
{
	/*
	Response is expected in this format
	OK
		or
	+CMS ERROR: <err>
	*/

	UserRequest *ur;
	const TcoreATResponse *resp = data;
	int response;
	const char *line = NULL;
	GSList *tokens=NULL;

	struct tresp_sms_set_cb_config respSetCbConfig = {0,};

	memset(&respSetCbConfig, 0, sizeof(struct tresp_sms_set_cb_config));

	ur = tcore_pending_ref_user_request(pending);

	if(resp->success > 0)
	{
		dbg("RESPONSE OK");

	}
	else
	{
		dbg("RESPONSE NOK");
		line = (const char*)resp->final_response;
		tokens = tcore_at_tok_new(line);

		if (g_slist_length(tokens) < 1) {
		  	dbg("err cause not specified or string corrupted");
		    	respSetCbConfig.result = TCORE_RETURN_3GPP_ERROR;
		}
		else
		{
			response = atoi(g_slist_nth_data(tokens, 0));
			/* TODO: CMEE error mapping is required. */
    			respSetCbConfig.result = TCORE_RETURN_3GPP_ERROR;
		}
	}
	if (!ur)
	{
		dbg("no user_request");
		return;
	}

	tcore_user_request_send_response(ur, TRESP_SMS_SET_CB_CONFIG, sizeof(struct tresp_sms_set_cb_config), &respSetCbConfig);

	return;
}

static void on_response_set_mem_status(TcorePending *p, int data_len, const void *data, void *user_data)
{
	UserRequest *ur;
	struct tresp_sms_set_mem_status respSetMemStatus = {0,};
	const TcoreATResponse *resp = data;

	memset(&respSetMemStatus, 0, sizeof(struct tresp_sms_set_mem_status));

	if(resp->success > 0)
	{
		dbg("RESPONSE OK");
		respSetMemStatus.result = SMS_SENDSMS_SUCCESS;

	}
	else
	{
		dbg("RESPONSE NOK");
		respSetMemStatus.result = SMS_DEVICE_FAILURE;
	}

	ur = tcore_pending_ref_user_request(p);
	if (!ur)
	{
		dbg("no user_request");
		return;
	}

	tcore_user_request_send_response(ur, TRESP_SMS_SET_MEM_STATUS, sizeof(struct tresp_sms_set_mem_status), &respSetMemStatus);

	return;
}

static void on_response_set_msg_status(TcorePending *pending, int data_len, const void *data, void *user_data)
{
	UserRequest *ur;
	struct tresp_sms_set_msg_status respMsgStatus = {0,};
	const TcoreATResponse *atResp = data;
        int response = 0, sw1 =0 , sw2 = 0;
        const char *line = NULL;
        char *pResp = NULL;
        GSList *tokens=NULL;

	dbg("Entry");

	memset(&respMsgStatus, 0, sizeof(struct tresp_sms_set_msg_status));
        ur = tcore_pending_ref_user_request(pending);

        if(atResp->success > 0)
        {
                dbg("RESPONSE OK");

		if(atResp->lines)
                {
                        line = (const char*)atResp->lines->data;
                        tokens = tcore_at_tok_new(line);
                        pResp = g_slist_nth_data(tokens, 0);
                        if (pResp != NULL)
                        {
                                sw1 = atoi(pResp);
                        }
                        else
                        {
                                respMsgStatus.result = SMS_DEVICE_FAILURE;
                                dbg("sw1 is NULL");
                        }
                        pResp = g_slist_nth_data(tokens, 1);
                        if (pResp != NULL)
                        {
                                sw2 = atoi(pResp);
                                if ((sw1 == AT_SW1_SUCCESS) && (sw2 == 0))
                                {
                                        respMsgStatus.result = SMS_SENDSMS_SUCCESS;
                                }
                                else
                                {
                                        //TODO Error Mapping
                                        respMsgStatus.result = SMS_DEVICE_FAILURE;
                                }
                        }
                        else
                        {
                                dbg("sw2 is NULL");
                                respMsgStatus.result = SMS_DEVICE_FAILURE;

       			}
                        pResp = g_slist_nth_data(tokens, 3);

        		if (pResp != NULL)
                        {
                                response = atoi(pResp);
                                dbg("response is %s", response);
                        }

                }
		else
		{
			dbg("No lines");
		}

        }
        else
        {
                dbg("RESPONSE NOK");
                respMsgStatus.result = SMS_DEVICE_FAILURE;
        }

        tcore_user_request_send_response(ur, TRESP_SMS_SET_MSG_STATUS , sizeof(struct tresp_sms_set_msg_status), &respMsgStatus);



	dbg("Exit");
	return;
}

static void on_response_get_sms_params(TcorePending *pending, int data_len, const void *data, void *user_data)
{
	UserRequest *ur;
	struct tresp_sms_get_params respGetParams;
	const TcoreATResponse *atResp = data;
	int response = 0, sw1 =0 , sw2 = 0;
	const char *line = NULL;
	char *pResp = NULL;
	GSList *tokens=NULL;

	memset(&respGetParams, 0, sizeof(struct tresp_sms_set_params));
	ur = tcore_pending_ref_user_request(pending);

	if(atResp->success > 0)
	{
		dbg("RESPONSE OK");

		if(atResp->lines)
		{
			line = (const char*)atResp->lines->data;
			tokens = tcore_at_tok_new(line);
			pResp = g_slist_nth_data(tokens, 0);
			if (pResp != NULL)
			{
				sw1 = atoi(pResp);
				dbg("sw1 is %d",sw1);
			}
			else
			{
				respGetParams.result = SMS_DEVICE_FAILURE;
				dbg("sw1 is NULL");
			}
			pResp = g_slist_nth_data(tokens, 1);
			if (pResp != NULL)
			{
				sw2 = atoi(pResp);
				//if ((sw1 == 144) && (sw2 == 0))
				if (sw1 == AT_SW1_SUCCESS)
				{
					respGetParams.result = SMS_SENDSMS_SUCCESS;
				}
				else
				{
					//TODO Error Mapping
					respGetParams.result = SMS_DEVICE_FAILURE;
				}
			}
			else
			{
				dbg("sw2 is NULL");
				respGetParams.result = SMS_DEVICE_FAILURE;

			}
			pResp = g_slist_nth_data(tokens, 3);
			if (pResp != NULL)
			{
				response = atoi(pResp);
				dbg("response is %s", response);
			}

		}
	}
	else
	{
		dbg("RESPONSE NOK");
		respGetParams.result = SMS_DEVICE_FAILURE;
	}

	dbg("Exit");
	return;
}

static void on_response_set_sms_params(TcorePending *pending, int data_len, const void *data, void *user_data)
{
	UserRequest *ur;
	struct tresp_sms_set_params respSetParams = {0,};
	const TcoreATResponse *atResp = data;
	int response = 0, sw1 =0 , sw2 = 0;
	const char *line = NULL;
	char *pResp = NULL;
	GSList *tokens=NULL;


	memset(&respSetParams, 0, sizeof(struct tresp_sms_set_params));
	ur = tcore_pending_ref_user_request(pending);

	if(atResp->success > 0)
	{
		dbg("RESPONSE OK");

		if(atResp->lines)
		{
			line = (const char*)atResp->lines->data;
			tokens = tcore_at_tok_new(line);
			pResp = g_slist_nth_data(tokens, 0);
			if (pResp != NULL)
			{
				sw1 = atoi(pResp);
			}
			else
			{
				respSetParams.result = SMS_DEVICE_FAILURE;
				dbg("sw1 is NULL");
			}
			pResp = g_slist_nth_data(tokens, 1);
			if (pResp != NULL)
			{
				sw2 = atoi(pResp);
				if ((sw1 == AT_SW1_SUCCESS) && (sw2 == AT_SW2_SUCCESS))
				{
					respSetParams.result = SMS_SENDSMS_SUCCESS;
				}
				else
				{
					//TODO Error Mapping
					respSetParams.result = SMS_DEVICE_FAILURE;
				}
			}
			else
			{
				dbg("sw2 is NULL");
				respSetParams.result = SMS_DEVICE_FAILURE;

			}
			pResp = g_slist_nth_data(tokens, 3);
			if (pResp != NULL)
			{
				response = atoi(pResp);
				dbg("response is %s", response);
			}

		}
	}
	else
	{
		dbg("RESPONSE NOK");
		respSetParams.result = SMS_DEVICE_FAILURE;
	}

	tcore_user_request_send_response(ur, TRESP_SMS_SET_PARAMS , sizeof(struct tresp_sms_set_params), &respSetParams);

	dbg("Exit");
	return;
}

static void on_response_get_paramcnt(TcorePending *p, int data_len, const void *data, void *user_data)
{

	UserRequest *ur = NULL;
	struct tresp_sms_get_paramcnt respGetParamCnt = {0,};
	const TcoreATResponse *atResp = data;
	char *line = NULL , *pResp = NULL;
	int sw1 = 0 , sw2 = 0;
	int sim_type = SIM_TYPE_USIM; //TODO need to check how to handle this
	GSList *tokens=NULL;

	dbg("Entry");

	ur = tcore_pending_ref_user_request(p);

	if(atResp->success > 0)
	{
		dbg("RESPONSE OK");

		if(atResp->lines)
		{
			line = (char*)atResp->lines->data;

			dbg("line is %s",line);

			tokens = tcore_at_tok_new(line);
			pResp = g_slist_nth_data(tokens, 0);
			if (pResp != NULL)
			{
				sw1 = atoi(pResp);
			}
			else
			{
				respGetParamCnt.result = SMS_DEVICE_FAILURE;
				dbg("sw1 is NULL");
			}
			pResp = g_slist_nth_data(tokens, 1);
			if (pResp != NULL)
			{
				sw2 = atoi(pResp);
				if ((sw1 == 144) && (sw2 == 0))
				{
					respGetParamCnt.result = SMS_SENDSMS_SUCCESS;
				}
				else
				{
					//TODO Error Mapping
					respGetParamCnt.result = SMS_DEVICE_FAILURE;
				}
			}
			else
			{
				dbg("sw2 is NULL");
				respGetParamCnt.result = SMS_DEVICE_FAILURE;

			}
			pResp = g_slist_nth_data(tokens, 3);
			if (pResp != NULL)
			{
				char *hexData;
				char *recordData;

				hexData  = pResp;
				if (pResp)
				dbg("response is %s", pResp);

				/*1. SIM access success case*/
				if ((sw1 == 0x90 && sw2 == 0x00) || sw1 == 0x91)
				{
					unsigned char tag_len = 0; /*	1 or 2 bytes ??? */
					unsigned short record_len = 0;
					char num_of_records = 0;
					unsigned char file_id_len = 0;
					unsigned short file_id = 0;
					unsigned short file_size = 0;
					unsigned short file_type = 0;
					unsigned short arr_file_id = 0;
					int arr_file_id_rec_num = 0;

					/*	handling only last 3 bits */
					unsigned char file_type_tag = 0x07;
					unsigned char *ptr_data;

					recordData = util_hexStringToBytes(hexData);
					util_hex_dump("    ", strlen(hexData)/2, recordData);

					ptr_data = (unsigned char *)recordData;

					if (sim_type ==  SIM_TYPE_USIM) {
						/*
						 ETSI TS 102 221 v7.9.0
				 			- Response Data
							 '62'	FCP template tag
							 - Response for an EF
							 '82'	M	File Descriptor
							 '83'	M	File Identifier
				 			'A5'	O	Proprietary information
							 '8A'	M	Life Cycle Status Integer
							 '8B', '8C' or 'AB'	C1	Security attributes
				 			'80'	M	File size
							 '81'	O	Total file size
				 			 '88'	O	Short File Identifier (SFI)
				 		*/

						/* rsim.res_len  has complete data length received  */

						/* FCP template tag - File Control Parameters tag*/
						if (*ptr_data == 0x62) {
							/* parse complete FCP tag*/
							/* increment to next byte */
							ptr_data++;
							tag_len = *ptr_data++;
							/* FCP file descriptor - file type, accessibility, DF, ADF etc*/
						if (*ptr_data == 0x82) {
								/* increment to next byte */
								ptr_data++;
								/*2 or 5 value*/
								ptr_data++;
						/*	unsigned char file_desc_len = *ptr_data++;*/
						/*	dbg("file descriptor length: [%d]", file_desc_len);*/
						/* TBD:  currently capture only file type : ignore sharable, non sharable, working, internal etc*/
						/* consider only last 3 bits*/
						file_type_tag = file_type_tag & (*ptr_data);

						switch (file_type_tag) {
							/* increment to next byte */
							ptr_data++;
							case 0x1:
								dbg("Getting FileType: [Transparent file type]");
								/* increment to next byte */
								ptr_data++;
								file_type = 0x01; 	//SIM_FTYPE_TRANSPARENT
								/*	data coding byte - value 21 */
								ptr_data++;
								break;

							case 0x2:
								dbg("Getting FileType: [Linear fixed file type]");
								/* increment to next byte */
								ptr_data++;
								/*	data coding byte - value 21 */
								ptr_data++;
								/*	2bytes */
								memcpy(&record_len, ptr_data, 2);
								/* swap bytes */
								SMS_SWAPBYTES16(record_len);
								ptr_data = ptr_data + 2;
								num_of_records = *ptr_data++;
								/* Data lossy conversation from enum (int) to unsigned char */
								file_type = 0x02;	// SIM_FTYPE_LINEAR_FIXED
								break;

							case 0x6:
								dbg(" Cyclic fixed file type");
								/* increment to next byte */
								ptr_data++;
								/*	data coding byte - value 21 */
								ptr_data++;
								/*	2bytes */
								memcpy(&record_len, ptr_data, 2);
								/* swap bytes  */
								SMS_SWAPBYTES16(record_len);
								ptr_data = ptr_data + 2;
								num_of_records = *ptr_data++;
								file_type = 0x04;	//SIM_FTYPE_CYCLIC
								break;

						default:
							dbg("not handled file type [0x%x]", *ptr_data);
							break;
						}
					}
					else
					{
						dbg("INVALID FCP received - DEbug!");
						return;
					}

					/*File identifier - file id?? */ // 0x84,0x85,0x86 etc are currently ignored and not handled
					if (*ptr_data == 0x83) {
						/* increment to next byte */
						ptr_data++;
						file_id_len = *ptr_data++;
						memcpy(&file_id, ptr_data, file_id_len);
						/* swap bytes	 */
						SMS_SWAPBYTES16(file_id);
						ptr_data = ptr_data + 2;
						dbg("Getting FileID=[0x%x]", file_id);
					} else {
						dbg("INVALID FCP received - DEbug!");
						free(recordData);
						//ReleaseResponse();
						return;
					}

					/*	proprietary information  */
					if (*ptr_data == 0xA5) {
						unsigned short prop_len;
						/* increment to next byte */
						ptr_data++;
						/* length */
						prop_len = *ptr_data;
						/* skip data */
						ptr_data = ptr_data + prop_len + 1;
					} else {
						dbg("INVALID FCP received - DEbug!");
					}

					/* life cycle status integer [8A][length:0x01][status]*/
					/*
					 status info b8~b1
					 00000000 : No information given
					 00000001 : creation state
					 00000011 : initialization state
					 000001-1 : operation state -activated
					 000001-0 : operation state -deactivated
					 000011-- : Termination state
					 b8~b5 !=0, b4~b1=X : Proprietary
					 Any other value : RFU
					 */
					if (*ptr_data == 0x8A) {
						/* increment to next byte */
						ptr_data++;
						/* length - value 1 */
						ptr_data++;

						switch (*ptr_data) {
							case 0x04:
							case 0x06:
								dbg("<IPC_RX> operation state -deactivated");
								ptr_data++;
								break;
							case 0x05:
							case 0x07:
								dbg("<IPC_RX> operation state -activated");
								ptr_data++;
								break;
							default:
								dbg("<IPC_RX> DEBUG! LIFE CYCLE STATUS =[0x%x]",*ptr_data);
								ptr_data++;
								break;
						}
					}

					/* related to security attributes : currently not handled*/
					if (*ptr_data == 0x86 || *ptr_data == 0x8B || *ptr_data == 0x8C || *ptr_data == 0xAB) {
						/* increment to next byte */
						ptr_data++;
						/* if tag length is 3 */
						if (*ptr_data == 0x03) {
							/* increment to next byte */
							ptr_data++;
							/* EFARR file id */
							memcpy(&arr_file_id, ptr_data, 2);
							/* swap byes */
							SMS_SWAPBYTES16(arr_file_id);
							ptr_data = ptr_data + 2;
							arr_file_id_rec_num = *ptr_data++;
						} else {
							/* if tag length is not 3 */
							/* ignoring bytes	*/
							//	ptr_data = ptr_data + 4;
							dbg("Useless security attributes, so jump to next tag");
							ptr_data = ptr_data + (*ptr_data + 1);
						}
					} else {
						dbg("INVALID FCP received[0x%x] - DEbug!", *ptr_data);
						free(recordData);

						return;
					}

					dbg("Current ptr_data value is [%x]", *ptr_data);

					/* file size excluding structural info*/
					if (*ptr_data == 0x80) {
						/* for EF file size is body of file and for Linear or cyclic it is
						 * number of recXsizeof(one record)
						 */
						/* increment to next byte */
						ptr_data++;
						/* length is 1 byte - value is 2 bytes or more */
						ptr_data++;
						memcpy(&file_size, ptr_data, 2);
						/* swap bytes */
						SMS_SWAPBYTES16(file_size);
						ptr_data = ptr_data + 2;
					} else {
						dbg("INVALID FCP received - DEbug!");
						free(recordData);
						return;
					}

					/* total file size including structural info*/
					if (*ptr_data == 0x81) {
						int len;
						/* increment to next byte */
						ptr_data++;
						/* length */
						len = *ptr_data;
						/* ignored bytes */
						ptr_data = ptr_data + 3;
					} else {
						dbg("INVALID FCP received - DEbug!");
						/* 0x81 is optional tag?? check out! so do not return -1 from here! */
						/* return -1; */
					}
					/*short file identifier ignored*/
					if (*ptr_data == 0x88) {
						dbg("0x88: Do Nothing");
						/*DO NOTHING*/
					}
				} else {
					dbg("INVALID FCP received - DEbug!");
					free(recordData);
					return;
				}
			}
			else if (sim_type == SIM_TYPE_GSM)
			{
				unsigned char gsm_specific_file_data_len = 0;
				/*	ignore RFU byte1 and byte2 */
				ptr_data++;
				ptr_data++;
				/*	file size */
				//file_size = p_info->response_len;
				memcpy(&file_size, ptr_data, 2);
				/* swap bytes */
				SMS_SWAPBYTES16(file_size);
				/*	parsed file size */
				ptr_data = ptr_data + 2;
				/*  file id  */
				memcpy(&file_id, ptr_data, 2);
				SMS_SWAPBYTES16(file_id);
				dbg(" FILE id --> [%x]", file_id);
				ptr_data = ptr_data + 2;
				/* save file type - transparent, linear fixed or cyclic */
				file_type_tag = (*(ptr_data + 7));

				switch (*ptr_data) {
					case 0x0:
						/* RFU file type */
						dbg(" RFU file type- not handled - Debug!");
						break;
					case 0x1:
						/* MF file type */
						dbg(" MF file type - not handled - Debug!");
						break;
					case 0x2:
						/* DF file type */
						dbg(" DF file type - not handled - Debug!");
						break;
					case 0x4:
						/* EF file type */
						dbg(" EF file type [%d] ", file_type_tag);
						/*	increment to next byte */
						ptr_data++;

						if (file_type_tag == 0x00 || file_type_tag == 0x01) {
							/* increament to next byte as this byte is RFU */
							ptr_data++;
							file_type =
									(file_type_tag == 0x00) ? 0x01 : 0x02; // SIM_FTYPE_TRANSPARENT:SIM_FTYPE_LINEAR_FIXED;
						} else {
							/* increment to next byte */
							ptr_data++;
							/*	For a cyclic EF all bits except bit 7 are RFU; b7=1 indicates that */
							/* the INCREASE command is allowed on the selected cyclic file. */
							file_type = 0x04;	// SIM_FTYPE_CYCLIC;
						}
						/* bytes 9 to 11 give SIM file access conditions */
						ptr_data++;
						/* byte 10 has one nibble that is RF U and another for INCREASE which is not used currently */
						ptr_data++;
						/* byte 11 is invalidate and rehabilate nibbles */
						ptr_data++;
						/* byte 12 - file status */
						ptr_data++;
						/* byte 13 - GSM specific data */
						gsm_specific_file_data_len = *ptr_data;
						ptr_data++;
						/*	byte 14 - structure of EF - transparent or linear or cyclic , already saved above */
						ptr_data++;
						/* byte 15 - length of record for linear and cyclic , for transparent it is set to 0x00. */
						record_len = *ptr_data;
						dbg("record length[%d], file size[%d]", record_len, file_size);

						if (record_len != 0)
							num_of_records = (file_size / record_len);

						dbg("Number of records [%d]", num_of_records);
						break;

					default:
						dbg(" not handled file type");
						break;
				}
			}
			else
			{
				dbg(" Card Type - UNKNOWN  [%d]", sim_type);
			}

			dbg("EF[0x%x] size[%ld] Type[0x%x] NumOfRecords[%ld] RecordLen[%ld]", file_id, file_size, file_type, num_of_records, record_len);

			respGetParamCnt.recordCount = num_of_records;
			respGetParamCnt.result = SMS_SUCCESS;

			free(recordData);
		}
		else
		{
			/*2. SIM access fail case*/
			dbg("SIM access fail");
			respGetParamCnt.result = SMS_UNKNOWN;
		}
		}else
		{
			dbg("presp is NULL");
		}

		}else
		{
			dbg("line is blank");
		}
	}
	else
	{
		dbg("RESPONSE NOK");
		respGetParamCnt.result = SMS_DEVICE_FAILURE;
	}


	tcore_user_request_send_response(ur, TRESP_SMS_GET_PARAMCNT, sizeof(struct tresp_sms_get_paramcnt), &respGetParamCnt);

	dbg("Exit");
	return;
}

/*=============================================================
							Requests
==============================================================*/
static TReturn send_umts_msg(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_send_umts_msg *sendUmtsMsg = NULL;

	char buf[512];
	int ScLength = 0;
	int pdu_len = 0;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	sendUmtsMsg = tcore_user_request_ref_data(ur, NULL);
  	hal = tcore_object_get_hal(obj);
	if(NULL == sendUmtsMsg || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("sendUmtsMsg: [%p], hal: [%p]", sendUmtsMsg, hal);

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	dbg("msgLength: [%d]", sendUmtsMsg->msgDataPackage.msgLength);
	util_hex_dump("    ", (SMS_SMDATA_SIZE_MAX+1), (void *)sendUmtsMsg->msgDataPackage.tpduData);
	util_hex_dump("    ", SMS_SMSP_ADDRESS_LEN, (void *)sendUmtsMsg->msgDataPackage.sca);

	ScLength = (int)sendUmtsMsg->msgDataPackage.sca[0];

	dbg("ScLength: [%d]", ScLength);

	if ((sendUmtsMsg->msgDataPackage.msgLength > 0)
		&& (sendUmtsMsg->msgDataPackage.msgLength <= SMS_SMDATA_SIZE_MAX)
		&& (ScLength <= SMS_MAX_SMS_SERVICE_CENTER_ADDR))
	{
		if(ScLength == 0) //ScAddress not specified
		{
			buf[0] = '0';
			buf[1] = '0';
			pdu_len = 2;
		}
		else
		{
		#ifndef TAPI_CODE_SUBJECT_TO_CHANGE
			dbg("SC length in Tx is %d - before", ScLength);

			util_sms_get_length_of_sca(&ScLength);

			dbg(" SC length in Tx is %d - after", ScLength);

			buf[0] = ScLength + 1;
			memcpy(&(buf[1]), &(sendUmtsMsg->msgDataPackage.sca[1]), (ScLength + 1));

			pdu_len = 2 + ScLength;
		#else
			dbg("Specifying SCA in TPDU is currently not supported");

			buf[0] = '0';
			buf[1] = '0';
			pdu_len = 2;
		#endif
		}

		util_byte_to_hex((const char *)sendUmtsMsg->msgDataPackage.tpduData, (char *)&buf[pdu_len], sendUmtsMsg->msgDataPackage.msgLength);

		pdu_len = pdu_len + 2*sendUmtsMsg->msgDataPackage.msgLength;

		buf[pdu_len] = '\0'; //Ensure termination

		dbg("pdu_len: [%d]", pdu_len);
		util_hex_dump("    ", sizeof(buf), (void *)buf);

		//AT+CMGS=<length><CR>PDU is given<ctrl-Z/ESC>
		cmd_str = g_strdup_printf("AT+CMGS=%d%s%s\x1A%s", sendUmtsMsg->msgDataPackage.msgLength, "\r", buf, "\r");
		atreq = tcore_at_request_new((const char *)cmd_str, "+CMGS:", TCORE_AT_SINGLELINE);
		pending = tcore_pending_new(obj, 0);

		if(NULL == cmd_str || NULL == atreq || NULL == pending)
		{
			err("Out of memory. Unable to proceed");
			dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

			//free memory we own
			g_free(cmd_str);
			util_sms_free_memory(atreq);
			util_sms_free_memory(pending);

			dbg("Exit");
			return TCORE_RETURN_ENOMEM;
		}

		util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

		tcore_pending_set_request_data(pending, 0, atreq);
		tcore_pending_set_response_callback(pending, on_response_send_umts_msg, NULL);
		tcore_pending_link_user_request(pending, ur);
		tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
		tcore_hal_send_request(hal, pending);

		g_free(cmd_str);

		dbg("Exit");
		return TCORE_RETURN_SUCCESS;
	}

	err("Invalid Data len");
	dbg("Exit");
	return TCORE_RETURN_SMS_INVALID_DATA_LEN;
}

static TReturn read_msg(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_read_msg *readMsg = NULL;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	readMsg = tcore_user_request_ref_data(ur, NULL);
	hal = tcore_object_get_hal(obj);
	if(NULL == readMsg || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("readMsg: [%p], hal: [%p]", readMsg, hal);

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	dbg("index: [%d]", readMsg->index);

	cmd_str = g_strdup_printf("AT+CMGR=%d\r", (readMsg->index + 1)); //IMC index is one ahead of TAPI
	atreq = tcore_at_request_new((const char *)cmd_str, "\e+CMGR:", TCORE_AT_SINGLELINE);
	pending = tcore_pending_new(obj, 0);

	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_response_callback(pending, on_response_read_msg, (void *)(uintptr_t)(readMsg->index)); //storing index as user data for response
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn save_msg(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_save_msg *saveMsg = NULL;
	int ScLength = 0, pdu_len = 0, stat = 0;
	char buf[512];

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	saveMsg = tcore_user_request_ref_data(ur, NULL);
	hal = tcore_object_get_hal(obj);
	if(NULL == saveMsg || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("saveMsg: [%p], hal: [%p]", saveMsg, hal);

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	dbg("Index value %d is not ensured. CP will allocate index automatically", saveMsg->simIndex);
	dbg("msgStatus: %x", saveMsg->msgStatus);
	dbg("msgLength: [%d]", saveMsg->msgDataPackage.msgLength);
	util_hex_dump("    ", (SMS_SMDATA_SIZE_MAX+1), (void *)saveMsg->msgDataPackage.tpduData);
	util_hex_dump("    ", SMS_SMSP_ADDRESS_LEN, (void *)saveMsg->msgDataPackage.sca);

	switch (saveMsg->msgStatus) {
		case SMS_STATUS_READ:
			stat = AT_REC_READ;
			break;

		case SMS_STATUS_UNREAD:
			stat = AT_REC_UNREAD;
			break;

		case SMS_STATUS_SENT:
			stat = AT_STO_SENT;
			break;

		case SMS_STATUS_UNSENT:
			stat = AT_STO_UNSENT;
			break;

		default:
			err("Invalid msgStatus");
			dbg("Exit");
			return TCORE_RETURN_EINVAL;
	}

	if ((saveMsg->msgDataPackage.msgLength > 0)
		&& (saveMsg->msgDataPackage.msgLength <= SMS_SMDATA_SIZE_MAX))
	{
		ScLength = (int)saveMsg->msgDataPackage.sca[0];

		if(ScLength == 0)
		{
			dbg("ScLength is zero");
			buf[0] = '0';
			buf[1] = '0';
			pdu_len = 2;
		}
		else {
		#ifndef TAPI_CODE_SUBJECT_TO_CHANGE
			dbg("ScLength (Useful semi-octets) %d", ScLength);

			util_sms_get_length_of_sca(&ScLength); /* Convert useful semi-octets to useful octets */

			dbg("ScLength (Useful octets) %d", ScLength);

			buf[0] = ScLength + 1; /* ScLength */
			memcpy(&(buf[1]), &(saveMsg->msgDataPackage.sca[1]), ScLength+1); /* ScType + ScAddress */

			pdu_len = 2 + ScLength;
		#endif
			dbg("Specifying SCA is currently not supported");
			buf[0] = '0';
			buf[1] = '0';
			pdu_len = 2;
		}

		util_byte_to_hex((const char *)saveMsg->msgDataPackage.tpduData, (char *)&(buf[pdu_len]), saveMsg->msgDataPackage.msgLength);

		pdu_len = pdu_len + 2*saveMsg->msgDataPackage.msgLength;

		buf[pdu_len] = '\0'; //Ensure termination

		dbg("pdu_len: [%d]", pdu_len);
		util_hex_dump("    ", sizeof(buf), (void *)buf);

		//AT+CMGW=<length>[,<stat>]<CR>PDU is given<ctrl-Z/ESC>
		cmd_str = g_strdup_printf("AT+CMGW=%d,%d%s%s\x1A%s", saveMsg->msgDataPackage.msgLength, stat, "\r", buf, "\r");
		pending = tcore_pending_new(obj, 0);
		atreq = tcore_at_request_new((const char *)cmd_str, "+CMGW", TCORE_AT_SINGLELINE);

		if(NULL == cmd_str || NULL == atreq || NULL == pending)
		{
			err("Out of memory. Unable to proceed");
			dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

			//free memory we own
			g_free(cmd_str);
			util_sms_free_memory(atreq);
			util_sms_free_memory(pending);

			dbg("Exit");
			return TCORE_RETURN_ENOMEM;
		}

		util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

		tcore_pending_set_request_data(pending, 0, atreq);
		tcore_pending_set_response_callback(pending, on_response_sms_save_msg_cnf, NULL);
		tcore_pending_link_user_request(pending, ur);
		tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
		tcore_hal_send_request(hal, pending);

		g_free(cmd_str);

		dbg("Exit");
		return TCORE_RETURN_SUCCESS;
	}

	err("Invalid Data len");
	dbg("Exit");
	return TCORE_RETURN_SMS_INVALID_DATA_LEN;
}

static TReturn delete_msg(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_delete_msg *deleteMsg = NULL;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	deleteMsg = tcore_user_request_ref_data(ur, NULL);
	hal = tcore_object_get_hal(obj);
	if(NULL == deleteMsg || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("deleteMsg: [%p], hal: [%p]", deleteMsg, hal);

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	dbg("index: %d", deleteMsg->index);

	cmd_str =g_strdup_printf("AT+CMGD=%d,0\r", deleteMsg->index);
	pending = tcore_pending_new(obj, 0);
	atreq = tcore_at_request_new((const char *)cmd_str, NULL, TCORE_AT_NO_RESULT);
	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_response_callback(pending, on_response_sms_delete_msg_cnf, (void *)(uintptr_t)(deleteMsg->index)); //storing index as user data for response
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn get_storedMsgCnt(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	hal = tcore_object_get_hal(obj);
	if(NULL == hal)
	{
		err("NULL HAL. Unable to proceed");

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	cmd_str = g_strdup_printf("AT+CPMS=\"SM\",\"SM\",\"SM\"%c", CR);
	pending = tcore_pending_new(obj, 0);
	atreq = tcore_at_request_new((const char *)cmd_str, "+CPMS", TCORE_AT_SINGLELINE);

	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_response_callback(pending, on_response_get_storedMsgCnt, NULL);
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn get_sca(CoreObject *obj, UserRequest *ur)
{
	gchar * cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_get_sca *getSca = NULL;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	getSca = tcore_user_request_ref_data(ur, NULL);
	hal = tcore_object_get_hal(obj);
	if(NULL == getSca || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("getSca: [%p], hal: [%p]", getSca, hal);

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	dbg("Index value %d is not ensured. CP doesn't support specifying index values for SCA", getSca->index);

	cmd_str = g_strdup_printf("AT+CSCA?\r");
	pending = tcore_pending_new(obj, 0);
	atreq = tcore_at_request_new((const char *)cmd_str, "+CSCA", TCORE_AT_SINGLELINE);

	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_response_callback(pending, on_response_get_sca, NULL);
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn set_sca(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_set_sca *setSca = NULL;
	int addrType = 0;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	setSca = tcore_user_request_ref_data(ur, NULL);
	hal = tcore_object_get_hal(obj);
	if(NULL == setSca || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("setSca: [%p], hal: [%p]", setSca, hal);

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	dbg("Index value %d is not ensured. CP will decide index automatically", setSca->index);
	dbg("dialNumLen: %u", setSca->scaInfo.dialNumLen);
	dbg("typeOfNum: %d", setSca->scaInfo.typeOfNum);
	dbg("numPlanId: %d", setSca->scaInfo.numPlanId);
	util_hex_dump("    ", (SMS_SMSP_ADDRESS_LEN+1), (void *)setSca->scaInfo.diallingNum);

	addrType = ((setSca->scaInfo.typeOfNum << 4) | setSca->scaInfo.numPlanId) | 0x80;

	cmd_str = g_strdup_printf("AT+CSCA=%s,%d", setSca->scaInfo.diallingNum, addrType);
	pending = tcore_pending_new(obj, 0);
	atreq = tcore_at_request_new((const char *)cmd_str, NULL, TCORE_AT_NO_RESULT);

	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_response_callback(pending, on_response_set_sca, NULL);
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn get_cb_config(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	hal = tcore_object_get_hal(obj);
	if(NULL == hal)
	{
		err("NULL HAL. Unable to proceed");

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	cmd_str = g_strdup_printf("AT+CSCB?");
	pending = tcore_pending_new(obj, 0);
	atreq = tcore_at_request_new((const char *)cmd_str, "+CSCB", TCORE_AT_SINGLELINE);
	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_response_callback(pending, on_response_get_cb_config, NULL);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_pending_link_user_request(pending, ur);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn set_cb_config(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	gchar *mids_str = NULL;
	GString *mids_GString = NULL;

	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_set_cb_config *setCbConfig = NULL;
	int loop_ctr = 0;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	setCbConfig = tcore_user_request_ref_data(ur, NULL);
	hal = tcore_object_get_hal(obj);
	if(NULL == setCbConfig || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("setCbConfig: [%p], hal: [%p]", setCbConfig, hal);

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	dbg("bCBEnabled: %d", setCbConfig->bCBEnabled);
	dbg("selectedId: %x", setCbConfig->selectedId);
	dbg("msgIdMaxCount: %x", setCbConfig->msgIdMaxCount);
	dbg("msgIdCount: %d", setCbConfig->msgIdCount);
	util_hex_dump("    ", SMS_GSM_SMS_CBMI_LIST_SIZE_MAX, (void *)setCbConfig->msgIDs);

	if (setCbConfig->bCBEnabled == FALSE) //AT+CSCB=0: Disable CBS
	{
		cmd_str = g_strdup_printf("AT+CSCB=0\r");
	}
	else
	{
		if(setCbConfig->selectedId == SMS_CBMI_SELECTED_SOME) //AT+CSCB=0,<mids>,<dcss>: Enable CBS for specified <mids> and <dcss>
		{
			dbg("Enabling specified CBMIs");
			mids_GString = g_string_new(g_strdup_printf("%d", setCbConfig->msgIDs[0]));

			for(loop_ctr=1; loop_ctr <setCbConfig->msgIdCount; loop_ctr++)
			{
				mids_GString = g_string_append(mids_GString, ",");
				mids_GString = g_string_append(mids_GString, g_strdup_printf("%d", setCbConfig->msgIDs[loop_ctr]));
			}

			mids_str = g_string_free(mids_GString, FALSE);
			cmd_str = g_strdup_printf("AT+CSCB=0,\"%s\"\r", mids_str);

			g_free(mids_str);
		}
		else if (setCbConfig->selectedId == SMS_CBMI_SELECTED_ALL) //AT+CSCB=1: Enable CBS for all <mids> and <dcss>
		{
			dbg("Enabling all CBMIs");
			cmd_str = g_strdup_printf("AT+CSCB=1\r");
		}
	}

	pending = tcore_pending_new(obj, 0);
	atreq = tcore_at_request_new((const char *)cmd_str, NULL, TCORE_AT_NO_RESULT);
	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_response_callback(pending, on_response_set_cb_config, NULL);
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn set_mem_status(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_set_mem_status *setMemStatus = NULL;
	int memoryStatus = 0;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	setMemStatus = tcore_user_request_ref_data(ur, NULL);
	hal = tcore_object_get_hal(obj);
	if(NULL == setMemStatus || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("setMemStatus: [%p], hal: [%p]", setMemStatus, hal);

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	dbg("memory_status: %d", setMemStatus->memory_status);

	if(setMemStatus->memory_status < SMS_PDA_MEMORY_STATUS_AVAILABLE
		|| setMemStatus->memory_status > SMS_PDA_MEMORY_STATUS_FULL)
	{
		err("Invalid memory_status");

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	switch (setMemStatus->memory_status)
	{
		case SMS_PDA_MEMORY_STATUS_AVAILABLE:
			memoryStatus = AT_MEMORY_AVAILABLE;
			break;

		case SMS_PDA_MEMORY_STATUS_FULL:
			memoryStatus = AT_MEMORY_FULL;
			break;

		default:
			err("Invalid memory_status");
			dbg("Exit");
			return TCORE_RETURN_EINVAL;
	}

	cmd_str = g_strdup_printf("AT+XTESM=%d", memoryStatus);
	pending = tcore_pending_new(obj, 0);
	atreq = tcore_at_request_new((const char *)cmd_str, NULL, TCORE_AT_NO_RESULT);

	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_response_callback(pending, on_response_set_mem_status, NULL);
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn get_pref_brearer(CoreObject *obj, UserRequest *ur)
{
	dbg("Entry");

	err("Functionality not implemented");

	dbg("Exit");
	return TCORE_RETURN_ENOSYS;
}

static TReturn set_pref_brearer(CoreObject *obj, UserRequest *ur)
{
	dbg("Entry");

	err("Functionality not implemented");

	dbg("Exit");
	return TCORE_RETURN_ENOSYS;
}

static TReturn set_delivery_report(CoreObject *obj, UserRequest *ur)
{
	dbg("Entry");

	err("CP takes care of SMS ack to network automatically. AP doesn't need to invoke this API.");

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn set_msg_status(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	char encoded_data[176];

	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_set_msg_status *msg_status = NULL;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	msg_status = tcore_user_request_ref_data(ur, NULL);
	hal = tcore_object_get_hal(obj);
	if(NULL == msg_status || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("msg_status: [%p], hal: [%p]", msg_status, hal);

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	dbg("msgStatus: [%x], index [%x]", msg_status->msgStatus, msg_status->index);

	switch (msg_status->msgStatus)
	{
		case SMS_STATUS_READ:
			encoded_data[0] = 0x01;
			break;

		case SMS_STATUS_UNREAD:
			encoded_data[0] = 0x03;
			break;

		case SMS_STATUS_UNSENT:
			encoded_data[0] = 0x07;
			break;

		case SMS_STATUS_SENT:
			encoded_data[0] = 0x05;
			break;

		case SMS_STATUS_DELIVERED:
			encoded_data[0] = 0x1D;
			break;

		case SMS_STATUS_DELIVERY_UNCONFIRMED:
			encoded_data[0] = 0xD;
			break;

		case SMS_STATUS_MESSAGE_REPLACED: //Fall Through
		case SMS_STATUS_RESERVED: //Fall Through
			encoded_data[0] = 0x03;
			break;
	}

	memset(&encoded_data[1], 0xff, 175);

	//AT+CRSM=command>[,<fileid>[,<P1>,<P2>,<P3>[,<data>[,<pathid>]]]]
	cmd_str = g_strdup_printf("AT+CRSM=220,28476,%d, 4, 176 %s\r", (msg_status->index+1), encoded_data);
	atreq = tcore_at_request_new((const char *)cmd_str, "+CRSM:", TCORE_AT_SINGLELINE);
	pending = tcore_pending_new(obj, 0);
	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_timeout(pending, 0);
	tcore_pending_set_response_callback(pending, on_response_set_msg_status, NULL);
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn get_sms_params(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_get_params *getSmsParams = NULL;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	getSmsParams = tcore_user_request_ref_data(ur, NULL);
	hal = tcore_object_get_hal(obj);
	if(NULL == getSmsParams || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("getSmsParams: [%p], hal: [%p]", getSmsParams, hal);

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	//AT+CRSM=command>[,<fileid>[,<P1>,<P2>,<P3>[,<data>[,<pathid>]]]]
	cmd_str = g_strdup_printf("AT+CRSM=178,28482,%d,4,28\r", (getSmsParams->index + 1));
	atreq = tcore_at_request_new((const char *)cmd_str, "+CRSM:", TCORE_AT_SINGLELINE);
	pending = tcore_pending_new(obj, 0);
	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_timeout(pending, 0);
	tcore_pending_set_response_callback(pending, on_response_get_sms_params, NULL);
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static TReturn set_sms_params(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	char *encoded_data = NULL;
	char *temp_data = NULL, *dest_addr = NULL, *svc_addr = NULL;

	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;
	const struct treq_sms_set_params *setSmsParams = NULL;
	int len = 0, alpha_id_len = 0,param_size = sizeof(struct treq_sms_set_params), encoded_data_len = 0;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	setSmsParams = tcore_user_request_ref_data(ur, NULL);
	hal = tcore_object_get_hal(obj);
	if(NULL == setSmsParams || NULL == hal)
	{
		err("NULL input. Unable to proceed");
		dbg("setSmsParams: [%p], hal: [%p]", setSmsParams, hal);

     	temp_data = calloc(param_size,1);

	alpha_id_len = setSmsParams->params.alphaIdLen;
	dbg("alpha id len is %d", alpha_id_len);

        memcpy(temp_data,&(setSmsParams->params.szAlphaId),alpha_id_len);
	memcpy((temp_data+alpha_id_len), &(setSmsParams->params.paramIndicator), 1);

	dest_addr = calloc(sizeof(struct telephony_sms_AddressInfo),1);
	svc_addr = calloc(sizeof(struct telephony_sms_AddressInfo),1);

	if (setSmsParams->params.tpDestAddr.dialNumLen == 0)
	{
		dbg("dest_addr is not present");
		dest_addr[0] = 0;

	}else
	{
		dest_addr[0] = setSmsParams->params.tpDestAddr.dialNumLen;
		dest_addr[1] = (((setSmsParams->params.tpDestAddr.typeOfNum << 4) | setSmsParams->params.tpDestAddr.numPlanId) | 0x80);
		memcpy(&(dest_addr[2]),setSmsParams->params.tpDestAddr.diallingNum,setSmsParams->params.tpDestAddr.dialNumLen);
		dbg("dest_addr is %s",dest_addr);

	}

       if (setSmsParams->params.tpSvcCntrAddr.dialNumLen == 0)
        {
                dbg("svc_addr is not present");
                svc_addr[0] = 0;

        }else
        {
                svc_addr[0] = setSmsParams->params.tpSvcCntrAddr.dialNumLen;
                svc_addr[1] = (((setSmsParams->params.tpSvcCntrAddr.typeOfNum << 4) | setSmsParams->params.tpSvcCntrAddr.numPlanId) | 0x80);
                memcpy(&(svc_addr[2]),&(setSmsParams->params.tpSvcCntrAddr.diallingNum),setSmsParams->params.tpSvcCntrAddr.dialNumLen);
                dbg("svc_addr is %s %s",svc_addr, setSmsParams->params.tpSvcCntrAddr.diallingNum);

        }

 	memcpy((temp_data+alpha_id_len+1), dest_addr, setSmsParams->params.tpDestAddr.dialNumLen);
        memcpy((temp_data+alpha_id_len+setSmsParams->params.tpDestAddr.dialNumLen+1), svc_addr, setSmsParams->params.tpSvcCntrAddr.dialNumLen);

        memcpy((temp_data+alpha_id_len+setSmsParams->params.tpDestAddr.dialNumLen+1+setSmsParams->params.tpSvcCntrAddr.dialNumLen), &(setSmsParams->params.tpProtocolId), 1);

        memcpy((temp_data+alpha_id_len+setSmsParams->params.tpDestAddr.dialNumLen+2+setSmsParams->params.tpSvcCntrAddr.dialNumLen), &(setSmsParams->params.tpDataCodingScheme), 1);

        memcpy((temp_data+alpha_id_len+setSmsParams->params.tpDestAddr.dialNumLen+3+setSmsParams->params.tpSvcCntrAddr.dialNumLen), &(setSmsParams->params.tpValidityPeriod), 1);
        len = strlen(temp_data);

	dbg("temp data and len %s %d",temp_data, len);


	//EFsmsp file size is 28 +Y bytes (Y is alpha id size)
        encoded_data = calloc((setSmsParams->params.alphaIdLen+28),1);

	util_byte_to_hex((const char *)temp_data, (char *)encoded_data,encoded_data_len);

	hal = tcore_object_get_hal(obj);
	pending = tcore_pending_new(obj, 0);

	if(NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("Out of memory. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		util_sms_free_memory(temp_data);
		util_sms_free_memory(encoded_data);

		dbg("Exit");
		return TCORE_RETURN_ENOMEM;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_timeout(pending, 0);
	tcore_pending_set_response_callback(pending, on_response_set_sms_params, NULL);
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);
	util_sms_free_memory(temp_data);
	util_sms_free_memory(encoded_data);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
	}
	return TRUE;
}

static TReturn get_paramcnt(CoreObject *obj, UserRequest *ur)
{
	gchar *cmd_str = NULL;
	TcoreHal *hal = NULL;
	TcoreATRequest *atreq = NULL;
	TcorePending *pending = NULL;

	dbg("Entry");
	dbg("CoreObject: [%p]", obj);
	dbg("UserRequest: [%p]", ur);

	hal = tcore_object_get_hal(obj);
	if(NULL == hal)
	{
		err("NULL HAL. Unable to proceed");

		dbg("Exit");
		return TCORE_RETURN_EINVAL;
	}

	//AT+CRSM=command>[,<fileid>[,<P1>,<P2>,<P3>[,<data>[,<pathid>]]]]
	cmd_str = g_strdup_printf("AT+CRSM=192, %d%s", 0x6F42, "\r");
	atreq = tcore_at_request_new((const char *)cmd_str, "+CRSM:", TCORE_AT_SINGLELINE);
	pending = tcore_pending_new(obj, 0);

	if (NULL == cmd_str || NULL == atreq || NULL == pending)
	{
		err("NULL pointer. Unable to proceed");
		dbg("cmd_str: [%p], atreq: [%p], pending: [%p]", cmd_str, atreq, pending);

		//free memory we own
		g_free(cmd_str);
		util_sms_free_memory(atreq);
		util_sms_free_memory(pending);

		dbg("Exit");
		return TCORE_RETURN_FAILURE;
	}

	util_hex_dump("    ", strlen(cmd_str), (void *)cmd_str);

	tcore_pending_set_request_data(pending, 0, atreq);
	tcore_pending_set_timeout(pending, 0);
	tcore_pending_set_response_callback(pending, on_response_get_paramcnt, NULL);
	tcore_pending_link_user_request(pending, ur);
	tcore_pending_set_send_callback(pending, on_confirmation_sms_message_send, NULL);
	tcore_hal_send_request(hal, pending);

	g_free(cmd_str);

	dbg("Exit");
	return TCORE_RETURN_SUCCESS;
}

static struct tcore_sms_operations sms_ops =
{
	.send_umts_msg = send_umts_msg,
	.read_msg = read_msg,
	.save_msg = save_msg,
	.delete_msg = delete_msg,
	.get_storedMsgCnt = get_storedMsgCnt,
	.get_sca = get_sca,
	.set_sca = set_sca,
	.get_cb_config = get_cb_config,
	.set_cb_config = set_cb_config,
	.set_mem_status = set_mem_status,
	.get_pref_brearer = get_pref_brearer,
	.set_pref_brearer = set_pref_brearer,
	.set_delivery_report = set_delivery_report,
	.set_msg_status = set_msg_status,
	.get_sms_params = get_sms_params,
	.set_sms_params = set_sms_params,
	.get_paramcnt = get_paramcnt,
};

gboolean s_sms_init(TcorePlugin *plugin, TcoreHal *hal)
{
	CoreObject *obj = NULL;
	struct property_sms_info *data = NULL;
	GQueue *work_queue = NULL;

	dbg("Entry");
	dbg("plugin: [%p]", plugin);
	dbg("hal: [%p]", hal);

	obj = tcore_sms_new(plugin, "umts_sms", &sms_ops, hal);

	data = calloc(sizeof(struct property_sms_info), 1);

	if (NULL == obj || NULL == data)
	{
		err("Unable to initialize. Exiting");
		s_sms_exit(plugin);

		dbg("Exit");
		return FALSE;
	}

	work_queue = g_queue_new();
	tcore_object_link_user_data(obj, work_queue);

	//Registering for SMS notifications
	tcore_object_add_callback(obj, "\e+CMTI", on_event_sms_incom_msg, NULL);
	tcore_object_add_callback(obj, "\e+CMT", on_event_sms_incom_msg, NULL);

	tcore_object_add_callback(obj, "\e+CDS", on_event_sms_incom_msg, NULL);
	tcore_object_add_callback(obj, "\e+CDSI", on_event_sms_incom_msg, NULL);

	tcore_object_add_callback(obj, "+XSMSMMSTAT", on_event_sms_memory_status, NULL);

	tcore_object_add_callback(obj, "\e+CBMI", on_event_sms_cb_incom_msg, NULL);
	tcore_object_add_callback(obj, "\e+CBM", on_event_sms_cb_incom_msg, NULL);
	tcore_object_add_callback(obj, "+XSIM", on_event_sms_ready_status, NULL);

	tcore_plugin_link_property(plugin, "SMS", data);

	dbg("Exit");
	return TRUE;
}

void s_sms_exit(TcorePlugin *plugin)
{
	CoreObject *obj = NULL;
	struct property_sms_info *data = NULL;

	dbg("Entry");
	dbg("plugin: [%p]", plugin);

	obj = tcore_plugin_ref_core_object(plugin, "umts_sms");
	if (NULL == obj)
	{
		err("NULL core object. Nothing to do.");
		return;
	}
	tcore_sms_free(obj);

	data = tcore_plugin_ref_property(plugin, "SMS");
	util_sms_free_memory(data);

	dbg("Exit");
	return;
}
