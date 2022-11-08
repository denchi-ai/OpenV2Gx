
/*******************************************************************
 * Command line interface for OpenV2G
 * Maintained in http://github.com/uhi22/OpenV2Gx, a fork of https://github.com/Martin-P/OpenV2G
 *
 ********************************************************************/


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <string.h>

#include "EXITypes.h"

#include "appHandEXIDatatypes.h"
#include "appHandEXIDatatypesEncoder.h"
#include "appHandEXIDatatypesDecoder.h"

/* Activate support for DIN */
#include "dinEXIDatatypes.h"
#if DEPLOY_DIN_CODEC == SUPPORT_YES
#include "dinEXIDatatypesEncoder.h"
#include "dinEXIDatatypesDecoder.h"
#endif /* DEPLOY_DIN_CODEC == SUPPORT_YES */

/* Activate support for XMLDSIG */
#include "xmldsigEXIDatatypes.h"
#if DEPLOY_XMLDSIG_CODEC == SUPPORT_YES
#include "xmldsigEXIDatatypesEncoder.h"
#include "xmldsigEXIDatatypesDecoder.h"
#endif /* DEPLOY_XMLDSIG_CODEC == SUPPORT_YES */

/* Activate support for ISO1 */
#include "iso1EXIDatatypes.h"
#if DEPLOY_ISO1_CODEC == SUPPORT_YES
#include "iso1EXIDatatypesEncoder.h"
#include "iso1EXIDatatypesDecoder.h"
#endif /* DEPLOY_ISO1_CODEC == SUPPORT_YES */

/* Activate support for ISO2 */
#include "iso2EXIDatatypes.h"
#if DEPLOY_ISO2_CODEC == SUPPORT_YES
#include "iso2EXIDatatypesEncoder.h"
#include "iso2EXIDatatypesDecoder.h"
#endif /* DEPLOY_ISO2_CODEC == SUPPORT_YES */

#include "v2gtp.h"

struct appHandEXIDocument aphsDoc;
struct dinEXIDocument dinDoc;
struct iso1EXIDocument iso1Doc;
struct iso2EXIDocument iso2Doc;

#define BUFFER_SIZE 256
uint8_t buffer1[BUFFER_SIZE];
uint8_t buffer2[BUFFER_SIZE];
bitstream_t global_stream1;
size_t global_pos1;
int g_errn;
char gResultString[4096];
char gInfoString[4096];
char gErrorString[4096];
char gPropertiesString[4096];
char s[1000];



#define ERROR_UNEXPECTED_REQUEST_MESSAGE -601
#define ERROR_UNEXPECTED_SESSION_SETUP_RESP_MESSAGE -602
#define ERROR_UNEXPECTED_SERVICE_DISCOVERY_RESP_MESSAGE -602
#define ERROR_UNEXPECTED_SERVICE_DETAILS_RESP_MESSAGE -603
#define ERROR_UNEXPECTED_PAYMENT_SERVICE_SELECTION_RESP_MESSAGE -604
#define ERROR_UNEXPECTED_PAYMENT_DETAILS_RESP_MESSAGE -605
#define ERROR_UNEXPECTED_AUTHORIZATION_RESP_MESSAGE -606
#define ERROR_UNEXPECTED_CHARGE_PARAMETER_DISCOVERY_RESP_MESSAGE -607
#define ERROR_UNEXPECTED_POWER_DELIVERY_RESP_MESSAGE -608
#define ERROR_UNEXPECTED_CHARGING_STATUS_RESP_MESSAGE -609
#define ERROR_UNEXPECTED_METERING_RECEIPT_RESP_MESSAGE -610
#define ERROR_UNEXPECTED_SESSION_STOP_RESP_MESSAGE -611
#define ERROR_UNEXPECTED_CABLE_CHECK_RESP_MESSAGE -612
#define ERROR_UNEXPECTED_PRE_CHARGE_RESP_MESSAGE -612
#define ERROR_UNEXPECTED_CURRENT_DEMAND_RESP_MESSAGE -613
#define ERROR_UNEXPECTED_WELDING_DETECTION_RESP_MESSAGE -614


/*
static int writeStringToEXIString(char* string, exi_string_character_t* exiString) {
	int pos = 0;
	while(string[pos]!='\0')
	{
		exiString[pos] = string[pos];
		pos++;
	}
	return pos;
}
*/

static void printASCIIString(exi_string_character_t* string, uint16_t len) {
	unsigned int i;
	char strShort[10];
	strcpy(s, "");
	for(i=0; i<len; i++) {
		sprintf(strShort, "%c",(char)string[i]);
		strcat(s, strShort);
	}
	//printf("\n");
}

/*
static void printBinaryArray(uint8_t* byte, uint16_t len) {
	unsigned int i;
	for(i=0; i<len; i++) {
		printf("%d ",byte[i]);
	}
	printf("\n");
}
*/

/*
static void copyBytes(uint8_t* from, uint16_t len, uint8_t* to) {
	int i;
	for(i=0; i<len; i++) {
		to[i] = from[i];
	}
}
*/

/* prepare an empty stream */
static void prepareGlobalStream(void) {
	global_stream1.size = BUFFER_SIZE;
	global_stream1.data = buffer1;
	global_stream1.pos = &global_pos1;	
	*(global_stream1.pos) = 0; /* start adding data at position 0 */	
}

/* print the global stream into the result string */
void printGlobalStream(void) {
	int i;
	char strTmp[10];	
	if (g_errn!=0) {
		sprintf(gErrorString, "encoding failed %d", g_errn);
	} else {
		strcpy(gResultString, "");
		/* byte per byte, write a two-character-hex value into the result string */
		for (i=0; i<*global_stream1.pos; i++) {
			sprintf(strTmp, "%02x", global_stream1.data[i]);
			strcat(gResultString, strTmp);
		}
	}	
}

void initProperties(void) {
	strcpy(gPropertiesString, "");
}

/* add to the JSON properties string a new line with name and value, e.g.
    , "responseCode": "ok"
*/	
void addProperty(char *strPropertyName, char *strPropertyValue) {
	strcat(gPropertiesString, ",\n\"");
	strcat(gPropertiesString, strPropertyName);
	strcat(gPropertiesString, "\": \"");
	strcat(gPropertiesString, strPropertyValue);
	strcat(gPropertiesString, "\"");	
}


/*********************************************************************************************************
*  Decoder --> JSON
**********************************************************************************************************
*  This chapter contains the four functions, to translate the C structs, which are filled by
*  the decoder, into JSON. Why four function? Because one for each schema: Handshake, Din, Iso1, Iso2.
*********************************************************************************************************/
 
/* translate the struct aphsDoc into JSON, to have it ready to give it over stdout to the caller application. */
void translateDocAppHandToJson(void) {
	int i;
	initProperties();
	addProperty("schema", "appHandshake");
	if (aphsDoc.supportedAppProtocolReq_isUsed) {
			/* it is a request */
			/* EVSE side: List of application handshake protocols of the EV */
			addProperty("msgName", "supportedAppProtocolReq");
			sprintf(s, "Vehicle supports %d protocols. ", aphsDoc.supportedAppProtocolReq.AppProtocol.arrayLen);
			strcat(gResultString, s);

			for(i=0;i<aphsDoc.supportedAppProtocolReq.AppProtocol.arrayLen;i++) {
				sprintf(s, "ProtocolEntry#%d ",(i+1));
				strcat(gResultString, s);
				sprintf(s, "ProtocolNamespace=");
				strcat(gResultString, s);
				printASCIIString(aphsDoc.supportedAppProtocolReq.AppProtocol.array[i].ProtocolNamespace.characters, aphsDoc.supportedAppProtocolReq.AppProtocol.array[i].ProtocolNamespace.charactersLen);
				strcat(gResultString, s);
				sprintf(s, " Version=%d.%d ", aphsDoc.supportedAppProtocolReq.AppProtocol.array[i].VersionNumberMajor, aphsDoc.supportedAppProtocolReq.AppProtocol.array[i].VersionNumberMinor);
				strcat(gResultString, s);
				sprintf(s, "SchemaID=%d ", aphsDoc.supportedAppProtocolReq.AppProtocol.array[i].SchemaID);
				strcat(gResultString, s);
				sprintf(s, "Priority=%d ", aphsDoc.supportedAppProtocolReq.AppProtocol.array[i].Priority);
				strcat(gResultString, s);
			}
	}
	if (aphsDoc.supportedAppProtocolRes_isUsed) {
			/* it is a response */
			addProperty("msgName", "supportedAppProtocolRes");
			sprintf(gResultString, "ResponseCode %d, SchemaID_isUsed %d, SchemaID %d",
				aphsDoc.supportedAppProtocolRes.ResponseCode,
				aphsDoc.supportedAppProtocolRes.SchemaID_isUsed,
				aphsDoc.supportedAppProtocolRes.SchemaID);
			//if (aphsDoc.supportedAppProtocolRes.SchemaID_isUsed) {
			//	SchemaID
			//}	
	}	
}

/* translate the struct dinDoc into JSON, to have it ready to give it over stdout to the caller application. */
void translateDocDinToJson(void) {
	char sTmp[20];
	initProperties();
	addProperty("schema", "DIN");
	sprintf(sTmp, "%d", g_errn);
	addProperty("g_errn", sTmp);
	
	/* unclear, which is the correct flag:
	   dinDoc.SessionSetupReq_isUsed or
	   dinDoc.V2G_Message.Body.SessionSetupReq_isUsed. <-- This works with the example EXI 809a0011d00000.
	*/   
	if (dinDoc.V2G_Message.Body.SessionSetupReq_isUsed) {
		addProperty("msgName", "SessionSetupReq");
	}
	if (dinDoc.V2G_Message.Body.SessionSetupRes_isUsed) {
		addProperty("msgName", "SessionSetupRes");
	}
	if (dinDoc.V2G_Message.Body.ServiceDiscoveryReq_isUsed) {
		addProperty("msgName", "ServiceDiscoveryReq");
	}
	if (dinDoc.V2G_Message.Body.ServiceDiscoveryRes_isUsed) {
		addProperty("msgName", "ServiceDiscoveryRes");
	}

	if (dinDoc.V2G_Message.Body.ServicePaymentSelectionReq_isUsed) {
		addProperty("msgName", "ServicePaymentSelectionReq");
	}
	if (dinDoc.V2G_Message.Body.ServicePaymentSelectionRes_isUsed) {
		addProperty("msgName", "ServicePaymentSelectionRes");
	}
	/* not supported in DIN 
	if (dinDoc.V2G_Message.Body.AuthorizationReq_isUsed) {
		addProperty("msgName", "AuthorizationReq");
	}
	if (dinDoc.V2G_Message.Body.AuthorizationRes_isUsed) {
		addProperty("msgName", "AuthorizationRes");
	}
	*/

	if (dinDoc.V2G_Message.Body.ChargeParameterDiscoveryReq_isUsed) {
		addProperty("msgName", "ChargeParameterDiscoveryReq");
	}
	if (dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes_isUsed) {
		addProperty("msgName", "ChargeParameterDiscoveryRes");
	}
	
	if (dinDoc.V2G_Message.Body.CableCheckReq_isUsed) {
		addProperty("msgName", "CableCheckReq");
	}
	if (dinDoc.V2G_Message.Body.CableCheckRes_isUsed) {
		addProperty("msgName", "CableCheckRes");
	}

	if (dinDoc.V2G_Message.Body.PreChargeReq_isUsed) {
		addProperty("msgName", "PreChargeReq");
	}
	if (dinDoc.V2G_Message.Body.PreChargeRes_isUsed) {
		addProperty("msgName", "PreChargeRes");
	}
}


/*********************************************************************************************************
*  Encoder
**********************************************************************************************************
*********************************************************************************************************/
static void encodeSupportedAppProtocolResponse(void) {
	/* The :supportedAppProtocolRes contains only two fields:
	    ResponseCode
		SchemaID (optional) It is not clear, in which cases the SchemaID can be left empty. Maybe if the list has only one enty. Or
		maybe always, to just select the first entry. */		
  init_appHandEXIDocument(&aphsDoc);
  aphsDoc.supportedAppProtocolRes_isUsed = 1u;
  aphsDoc.supportedAppProtocolRes.ResponseCode = appHandresponseCodeType_OK_SuccessfulNegotiation;
  aphsDoc.supportedAppProtocolRes.SchemaID = 1; /* todo: fill with one of the SchemaIDs of the request. The Ioniq uses schema 1 as one-and-only. */
  aphsDoc.supportedAppProtocolRes.SchemaID_isUsed = 1u; /* todo: shall we try without SchemaID? */

  prepareGlobalStream();
  g_errn = encode_appHandExiDocument(&global_stream1, &aphsDoc);
  printGlobalStream();
  sprintf(gInfoString, "encodeSupportedAppProtocolResponse finished");
}

static void encodeSessionSetupRequest(void) {
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.SessionSetupReq_isUsed = 1u;
	init_dinSessionSetupReqType(&dinDoc.V2G_Message.Body.SessionSetupReq);
	prepareGlobalStream();
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodeSessionSetupRequest finished");
}

static void encodeSessionSetupResponse(void) {
	dinDoc.V2G_Message_isUsed = 1u;
	/* generate an unique sessionID */
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	dinDoc.V2G_Message.Header.SessionID.bytes[0] = 1;
	dinDoc.V2G_Message.Header.SessionID.bytes[1] = 2;
	dinDoc.V2G_Message.Header.SessionID.bytes[2] = 3;
	dinDoc.V2G_Message.Header.SessionID.bytes[3] = 4;
	dinDoc.V2G_Message.Header.SessionID.bytes[4] = 5;
	dinDoc.V2G_Message.Header.SessionID.bytes[5] = 6;
	dinDoc.V2G_Message.Header.SessionID.bytes[6] = 7;
	dinDoc.V2G_Message.Header.SessionID.bytes[7] = 8;
	dinDoc.V2G_Message.Header.SessionID.bytesLen = 8;
	/* Prepare data for EV */
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.SessionSetupRes_isUsed = 1u;
	init_dinSessionSetupResType(&dinDoc.V2G_Message.Body.SessionSetupRes);
	dinDoc.V2G_Message.Body.SessionSetupRes.ResponseCode = dinresponseCodeType_OK;
	//dinDoc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[0] = 0;
	//dinDoc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[1] = 20;
	//dinDoc.V2G_Message.Body.SessionSetupRes.EVSEID.charactersLen = 2;
	//dinDoc.V2G_Message.Body.SessionSetupRes.EVSETimeStamp_isUsed = 0u;
	//dinDoc.V2G_Message.Body.SessionSetupRes.EVSETimeStamp = 123456789;
	prepareGlobalStream();
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodeSessionSetupResponse finished");
}

static void encodeServiceDiscoveryRequest(void) {
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.ServiceDiscoveryReq_isUsed = 1u;
	init_dinServiceDiscoveryReqType(&dinDoc.V2G_Message.Body.ServiceDiscoveryReq);
	prepareGlobalStream();
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodeServiceDiscoveryRequest finished");
}

void encodeServiceDiscoveryResponse(void) {
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.ServiceDiscoveryRes_isUsed = 1u;
	init_dinServiceDiscoveryResType(&dinDoc.V2G_Message.Body.ServiceDiscoveryRes);
	dinDoc.V2G_Message.Body.ServiceDiscoveryRes.ResponseCode = dinresponseCodeType_OK;
	/* the mandatory fields in the ISO are PaymentOptionList and ChargeService.
	   But in the DIN, this is different, we find PaymentOptions, ChargeService and optional ServiceList */
	dinDoc.V2G_Message.Body.ServiceDiscoveryRes.PaymentOptions.PaymentOption.array[0] = dinpaymentOptionType_ExternalPayment; /* EVSE handles the payment */
	dinDoc.V2G_Message.Body.ServiceDiscoveryRes.PaymentOptions.PaymentOption.arrayLen = 1; /* just one single payment option in the table */
	dinDoc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceID = 1; /* todo: not clear what this means  */
	//dinDoc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceName
	//dinDoc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceName_isUsed
	dinDoc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceCategory = dinserviceCategoryType_EVCharging;
	//dinDoc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceScope
	//dinDoc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceScope_isUsed
	dinDoc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.FreeService = 1; /* what ever this means. Just from example. */
	/* dinEVSESupportedEnergyTransferType, e.g.
								dinEVSESupportedEnergyTransferType_DC_combo_core or
								dinEVSESupportedEnergyTransferType_AC_single_phase_core */
	dinDoc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.EnergyTransferType = dinEVSESupportedEnergyTransferType_DC_combo_core;
	
	prepareGlobalStream();
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodeServiceDiscoveryResponse finished");
}

static void encodeServicePaymentSelectionRequest(void) {
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.ServicePaymentSelectionReq_isUsed = 1u;
	init_dinServicePaymentSelectionReqType(&dinDoc.V2G_Message.Body.ServicePaymentSelectionReq);
	/* the mandatory fields in ISO are SelectedPaymentOption and SelectedServiceList. Same in DIN. */
	dinDoc.V2G_Message.Body.ServicePaymentSelectionReq.SelectedPaymentOption = dinpaymentOptionType_ExternalPayment; /* not paying per car */
	dinDoc.V2G_Message.Body.ServicePaymentSelectionReq.SelectedServiceList.SelectedService.array[0].ServiceID = 1; /* todo: what ever this means */
	dinDoc.V2G_Message.Body.ServicePaymentSelectionReq.SelectedServiceList.SelectedService.arrayLen = 1; /* just one element in the array */ 
	prepareGlobalStream();
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodeServicePaymentSelectionRequest finished");
}


void encodeServicePaymentSelectionResponse(void) {
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.ServicePaymentSelectionRes_isUsed = 1u;
	init_dinServicePaymentSelectionResType(&dinDoc.V2G_Message.Body.ServicePaymentSelectionRes);
	prepareGlobalStream();
	/* todo: Encoder fails, error code EXI_ERROR_UNKOWN_EVENT -109. Maybe not supported? Not necessary??? Or just parameters missing... */
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodeServicePaymentSelectionResponse finished");
}

/* not in DIN
void encodeAuthorizationResponse(void) {
	sprintf(gErrorString, "todo encodeAuthorizationResponse");
}
*/

static void encodeChargeParameterDiscoveryRequest(void) {
	struct dinDC_EVChargeParameterType *cp;
	
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.ChargeParameterDiscoveryReq_isUsed = 1u;
	init_dinChargeParameterDiscoveryReqType(&dinDoc.V2G_Message.Body.ChargeParameterDiscoveryReq);
	dinDoc.V2G_Message.Body.ChargeParameterDiscoveryReq.EVRequestedEnergyTransferType = dinEVRequestedEnergyTransferType_DC_combo_core;
	//dinDoc.V2G_Message.Body.ChargeParameterDiscoveryReq.EVChargeParameter
	//dinDoc.V2G_Message.Body.ChargeParameterDiscoveryReq.EVChargeParameter_isUsed = 1;
	cp = &dinDoc.V2G_Message.Body.ChargeParameterDiscoveryReq.DC_EVChargeParameter;
	cp->DC_EVStatus.EVReady = 1; /* todo: what ever this means */
	//cp->DC_EVStatus.EVCabinConditioning
	//cp->DC_EVStatus.EVCabinConditioning_isUsed
	cp->DC_EVStatus.EVRESSSOC = 55; /* state of charge, 0 ... 100 in percent */
	cp->EVMaximumCurrentLimit.Multiplier = 0; /* -3 to 3. The exponent for base of 10. */
	cp->EVMaximumCurrentLimit.Unit = dinunitSymbolType_A;
	cp->EVMaximumCurrentLimit.Value = 100; 
	//cp->EVMaximumPowerLimit ;
	//cp->EVMaximumPowerLimit_isUsed:1;
	cp->EVMaximumVoltageLimit.Multiplier = 0;  /* -3 to 3. The exponent for base of 10. */
	cp->EVMaximumVoltageLimit.Unit = dinunitSymbolType_V;
	cp->EVMaximumVoltageLimit.Value = 398;
	//cp->EVEnergyCapacity
	//cp->EVEnergyCapacity_isUsed
	//cp->EVEnergyRequest
	//cp->EVEnergyRequest_isUsed
	//cp->FullSOC
	//cp->FullSOC_isUsed
	//cp->BulkSOC
	//cp->BulkSOC_isUsed
	dinDoc.V2G_Message.Body.ChargeParameterDiscoveryReq.DC_EVChargeParameter_isUsed = 1;
	prepareGlobalStream();
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodeChargeParameterDiscoveryRequest finished");
}

void encodeChargeParameterDiscoveryResponse(void) {
	struct dinEVSEChargeParameterType *cp;
	struct dinDC_EVSEChargeParameterType *cpdc;
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes_isUsed = 1u;
	init_dinChargeParameterDiscoveryResType(&dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes);
	dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes.ResponseCode = dinresponseCodeType_OK;
	/*
		dinEVSEProcessingType_Finished = 0,
	    dinEVSEProcessingType_Ongoing = 1
	*/
	dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes.EVSEProcessing = dinEVSEProcessingType_Finished;
	/* The encoder wants either SASchedules or SAScheduleList. If both are missing, it fails. (around line 3993 in dinEXIDatatypesEncoder.c).
	   The content is not used at all, but we just set it, to satisfy the encoder. */
	dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes.SASchedules.noContent = 0;
	dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes.SASchedules_isUsed = 1;
	
	cp = &dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes.EVSEChargeParameter;
	cp->noContent = 0;
	dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes.EVSEChargeParameter_isUsed=0;
	
	//dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes.AC_EVSEChargeParameter
	//dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes.AC_EVSEChargeParameter_isUsed
	cpdc = &dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter;
	cpdc->DC_EVSEStatus.EVSEIsolationStatus = dinisolationLevelType_Valid;
	cpdc->DC_EVSEStatus.EVSEIsolationStatus_isUsed = 1;
	cpdc->DC_EVSEStatus.EVSEStatusCode = dinDC_EVSEStatusCodeType_EVSE_Ready;
	cpdc->DC_EVSEStatus.NotificationMaxDelay = 0; /* expected time until the PEV reacts on the below mentioned notification. Not relevant. */
	cpdc->DC_EVSEStatus.EVSENotification = dinEVSENotificationType_None; /* could also be dinEVSENotificationType_StopCharging */


	cpdc->EVSEMaximumCurrentLimit.Multiplier = 0;  /* -3 to 3. The exponent for base of 10. */
	cpdc->EVSEMaximumCurrentLimit.Unit = dinunitSymbolType_A;
	cpdc->EVSEMaximumCurrentLimit.Value = 200;
	
	cpdc->EVSEMaximumPowerLimit.Multiplier = 3;  /* -3 to 3. The exponent for base of 10. */
	cpdc->EVSEMaximumPowerLimit.Unit = dinunitSymbolType_W;
	cpdc->EVSEMaximumPowerLimit.Value = 10;
	cpdc->EVSEMaximumPowerLimit_isUsed = 1;
	
	cpdc->EVSEMaximumVoltageLimit.Multiplier = 0;  /* -3 to 3. The exponent for base of 10. */
	cpdc->EVSEMaximumVoltageLimit.Unit = dinunitSymbolType_V;
	cpdc->EVSEMaximumVoltageLimit.Value = 450;
	
	cpdc->EVSEMinimumCurrentLimit.Multiplier = 0;  /* -3 to 3. The exponent for base of 10. */
	cpdc->EVSEMinimumCurrentLimit.Unit = dinunitSymbolType_A;
	cpdc->EVSEMinimumCurrentLimit.Value = 1;
	
	cpdc->EVSEMinimumVoltageLimit.Multiplier = 0;  /* -3 to 3. The exponent for base of 10. */
	cpdc->EVSEMinimumVoltageLimit.Unit = dinunitSymbolType_V;
	cpdc->EVSEMinimumVoltageLimit.Value = 200;
	
	cpdc->EVSECurrentRegulationTolerance.Multiplier = 0;  /* -3 to 3. The exponent for base of 10. */
	cpdc->EVSECurrentRegulationTolerance.Unit = dinunitSymbolType_A;
	cpdc->EVSECurrentRegulationTolerance.Value = 5;
	cpdc->EVSECurrentRegulationTolerance_isUsed=1;
	
	cpdc->EVSEPeakCurrentRipple.Multiplier = 0;  /* -3 to 3. The exponent for base of 10. */
	cpdc->EVSEPeakCurrentRipple.Unit = dinunitSymbolType_A;
	cpdc->EVSEPeakCurrentRipple.Value = 5;	
	//cpdc->EVSEEnergyToBeDelivered ;
	//cpdc->EVSEEnergyToBeDelivered_isUsed:1;
	dinDoc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed = 1;
	prepareGlobalStream();
	/* todo: Encoder fails, error code EXI_ERROR_UNKOWN_EVENT -109. Maybe not supported? Not necessary??? Or just parameters missing... */
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodeChargeParameterDiscoveryResponse finished");
}

static void encodeCableCheckRequest(void) {
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.CableCheckReq_isUsed = 1u;
	init_dinCableCheckReqType(&dinDoc.V2G_Message.Body.CableCheckReq);
	prepareGlobalStream();
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodeCableCheckRequest finished");
}

void encodeCableCheckResponse(void) {
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.CableCheckRes_isUsed = 1u;
	init_dinCableCheckResType(&dinDoc.V2G_Message.Body.CableCheckRes);
	prepareGlobalStream();
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodeCableCheckResponse finished");
}

static void encodePreChargeRequest(void) {
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.PreChargeReq_isUsed = 1u;
	init_dinPreChargeReqType(&dinDoc.V2G_Message.Body.PreChargeReq);
	prepareGlobalStream();
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodePreChargeRequest finished");
}

void encodePreChargeResponse(void) {
	dinDoc.V2G_Message_isUsed = 1u;
	init_dinMessageHeaderType(&dinDoc.V2G_Message.Header);
	init_dinBodyType(&dinDoc.V2G_Message.Body);
	dinDoc.V2G_Message.Body.PreChargeRes_isUsed = 1u;
	init_dinPreChargeResType(&dinDoc.V2G_Message.Body.PreChargeRes);
	prepareGlobalStream();
	g_errn = encode_dinExiDocument(&global_stream1, &dinDoc);
    printGlobalStream();
    sprintf(gInfoString, "encodePreChargeResponse finished");
}


/* Converting parameters to an EXI stream */
static void runTheEncoder(char* parameterStream) {
  //printf("runTheEncoder\n");
  /* Parameter description: Three letters:
      - First letter: E=Encode
	  - Second letter: Schema selection H=Handshake, D=DIN, 1=ISO1, 2=ISO2
	  - Third letter: A to Z for requests, a to z for responses.
  */
  switch (parameterStream[1]) { /* H=HandshakeRequest, h=HandshakeResponse, D=DIN, 1=ISO1, 2=ISO2 */
	case 'H':
		//encodeSupportedAppProtocolRequest();
		sprintf(gErrorString, "encodeSupportedAppProtocolRequest not yet implemented.");
		break;
	case 'h':
		encodeSupportedAppProtocolResponse();
		break;
	case 'D':
		switch (parameterStream[2]) {
			case 'A':
				encodeSessionSetupRequest();
				break;
			case 'a':
				encodeSessionSetupResponse();
				break;
			case 'B':
				encodeServiceDiscoveryRequest();
				break;
			case 'b':
				encodeServiceDiscoveryResponse();
				break;
			case 'C':
				encodeServicePaymentSelectionRequest(); /* DIN name is ServicePaymentSelection, but ISO name is PaymentServiceSelection */
				break;
			case 'c':
				encodeServicePaymentSelectionResponse(); /* DIN name is ServicePaymentSelection, but ISO name is PaymentServiceSelection */
				break;
			case 'D':
				sprintf(gErrorString, "AuthorizationRequest not specified for DIN.");
				break;
			case 'd':
				sprintf(gErrorString, "AuthorizationResponse not specified for DIN.");
				//encodeAuthorizationResponse();
				break;
			case 'E':
				encodeChargeParameterDiscoveryRequest();
				break;
			case 'e':
				encodeChargeParameterDiscoveryResponse();
				break;
			case 'F':
				encodeCableCheckRequest();
				break;
			case 'f':
				encodeCableCheckResponse();
				break;
			case 'G':
				encodePreChargeRequest();
				break;
			case 'g':
				encodePreChargeResponse();
				break;
			default:
				sprintf(gErrorString, "invalid message in DIN encoder requested");
		}
		break;
	case '1':
		sprintf(gErrorString, "ISO1 encoder not yet implemented");
		break;
	case '2':
		sprintf(gErrorString, "ISO2 encoder not yet implemented");
		break;
	default:
		sprintf(gErrorString, "invalid encoder requested");
  }
}

/** Converting EXI stream to parameters  */
static void runTheDecoder(char* parameterStream) {
	int i;
	int numBytes;
	char strOneByteHex[3];

	if (strlen(parameterStream)<4) {
		/* minimum is 4 characters, e.g. DH01 */
		sprintf(gErrorString, "parameter too short");
		return;
	}
	/*** step 1: convert the hex string into an array of bytes ***/
	global_stream1.size = BUFFER_SIZE;
	global_stream1.data = buffer1;
	global_stream1.pos = &global_pos1;
	numBytes=strlen(parameterStream)/2; /* contains one "virtual byte e.g. DH" at the beginning, which does not belong to the payload. */
	global_pos1 = 0;
	strOneByteHex[2]=0;
	/* convert the hex stream into array of bytes: */
	for (i=1; i<numBytes; i++) { /* starting at 1, means the first two characters (the direction-and-schema-selectors) are jumped-over. */
		strOneByteHex[0] = parameterStream[2*i];
		strOneByteHex[1] = parameterStream[2*i+1];
		buffer1[global_pos1++] = strtol(strOneByteHex, NULL, 16); /* convert the hex representation into a byte value */
	}
	sprintf(gInfoString, "%d bytes to convert", global_pos1);
	/*
	printf("size = %d\n", global_stream1.size);
	printf("pos  = %d\n", *(global_stream1.pos));
	for (i=0; i<(*global_stream1.pos); i++) {
		printf("%02x ", global_stream1.data[i]);
	}
	printf("\n");
	*/
	*(global_stream1.pos) = 0; /* the decoder shall start at the byte 0 */	
	
	/*** step 2: decide about which schema to use, and call the related decoder ***/
	/* The second character selects the schema. */
	/* The OpenV2G supports 4 different decoders:
	    decode_appHandExiDocument
		decode_iso1ExiDocument
		decode_iso2ExiDocument
		decode_dinExiDocument
		The caller needs to choose, depending on its context-knowledge, the correct decoder.
		The first step in a session is always to use the decode_appHandExiDocument for finding out, which specification/decoder
		is used in the next steps. */
	g_errn = 0;
	switch (parameterStream[1]) {
		case 'H': /* for the decoder, it does not matter whether it is a handshake request (H) or handshake response (h).
		             The same decoder schema is used. */
		case 'h':
			g_errn = decode_appHandExiDocument(&global_stream1, &aphsDoc);
			translateDocAppHandToJson();
			break;
		case 'D': /* The DIN schema decoder */
			g_errn = decode_dinExiDocument(&global_stream1, &dinDoc);
			translateDocDinToJson();
			break;
		case '1': /* The ISO1 schema decoder */
			g_errn = decode_iso1ExiDocument(&global_stream1, &iso1Doc);
			//translateDocIso1ToJson();
			break;
		case '2': /* The ISO2 schema decoder */
			g_errn = decode_iso2ExiDocument(&global_stream1, &iso2Doc);
			//translateDocIso2ToJson();
			break;
		default:
			sprintf(gErrorString, "The second character of the parameter must be H for applicationHandshake, D for DIN, 1 for ISO1 or 2 for ISO2.");
			g_errn = -1;
	}
			
	if(g_errn) {
			/* an error occured */
			sprintf(gErrorString, "runTheDecoder error%d", g_errn);
	}		
}

/* The entry point */
int main_commandline(int argc, char *argv[]) {
	strcpy(gInfoString, "");
	strcpy(gErrorString, "");
	strcpy(gResultString, "");
	strcpy(gPropertiesString, "");
	if (argc>=2) {
		//printf("OpenV2G will process %s\n", argv[1]);
		/* The first char of the parameter decides over Encoding or Decoding. */
		if (argv[1][0]=='E') {
			runTheEncoder(argv[1]);
		} else if (argv[1][0]=='D') {
			runTheDecoder(argv[1]);
		} else {
			sprintf(gErrorString, "The first character of the parameter must be D for decoding or E for encoding.");
		}
	} else {
		sprintf(gErrorString, "OpenV2G: Error: To few parameters.");
	}
	printf("{\n\"info\": \"%s\", \n\"error\": \"%s\",\n\"result\": \"%s\"%s\n}", gInfoString, gErrorString, gResultString, gPropertiesString);
	//printf("%s\n", gResultString);
	return 0;
}


