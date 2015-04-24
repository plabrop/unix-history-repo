/*
 * Generic advertisement service (GAS) server
 * Copyright (c) 2011-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef GAS_SERV_H
#define GAS_SERV_H

#define ANQP_REQ_CAPABILITY_LIST \
	(1 << (ANQP_CAPABILITY_LIST - ANQP_QUERY_LIST))
#define ANQP_REQ_VENUE_NAME \
	(1 << (ANQP_VENUE_NAME - ANQP_QUERY_LIST))
#define ANQP_REQ_NETWORK_AUTH_TYPE \
	(1 << (ANQP_NETWORK_AUTH_TYPE - ANQP_QUERY_LIST))
#define ANQP_REQ_ROAMING_CONSORTIUM \
	(1 << (ANQP_ROAMING_CONSORTIUM - ANQP_QUERY_LIST))
#define ANQP_REQ_IP_ADDR_TYPE_AVAILABILITY \
	(1 << (ANQP_IP_ADDR_TYPE_AVAILABILITY - ANQP_QUERY_LIST))
#define ANQP_REQ_NAI_REALM \
	(1 << (ANQP_NAI_REALM - ANQP_QUERY_LIST))
#define ANQP_REQ_3GPP_CELLULAR_NETWORK \
	(1 << (ANQP_3GPP_CELLULAR_NETWORK - ANQP_QUERY_LIST))
#define ANQP_REQ_DOMAIN_NAME \
	(1 << (ANQP_DOMAIN_NAME - ANQP_QUERY_LIST))
#define ANQP_REQ_HS_CAPABILITY_LIST \
	(0x10000 << HS20_STYPE_CAPABILITY_LIST)
#define ANQP_REQ_OPERATOR_FRIENDLY_NAME \
	(0x10000 << HS20_STYPE_OPERATOR_FRIENDLY_NAME)
#define ANQP_REQ_WAN_METRICS \
	(0x10000 << HS20_STYPE_WAN_METRICS)
#define ANQP_REQ_CONNECTION_CAPABILITY \
	(0x10000 << HS20_STYPE_CONNECTION_CAPABILITY)
#define ANQP_REQ_NAI_HOME_REALM \
	(0x10000 << HS20_STYPE_NAI_HOME_REALM_QUERY)
#define ANQP_REQ_OPERATING_CLASS \
	(0x10000 << HS20_STYPE_OPERATING_CLASS)
#define ANQP_REQ_OSU_PROVIDERS_LIST \
	(0x10000 << HS20_STYPE_OSU_PROVIDERS_LIST)
#define ANQP_REQ_ICON_REQUEST \
	(0x10000 << HS20_STYPE_ICON_REQUEST)

struct gas_dialog_info {
	u8 valid;
	struct wpabuf *sd_resp; /* Fragmented response */
	u8 dialog_token;
	size_t sd_resp_pos; /* Offset in sd_resp */
	u8 sd_frag_id;
	int prot; /* whether Protected Dual of Public Action frame is used */
};

struct hostapd_data;

struct gas_dialog_info *
gas_serv_dialog_find(struct hostapd_data *hapd, const u8 *addr,
		     u8 dialog_token);
void gas_serv_dialog_clear(struct gas_dialog_info *dialog);

int gas_serv_init(struct hostapd_data *hapd);
void gas_serv_deinit(struct hostapd_data *hapd);

#endif /* GAS_SERV_H */
