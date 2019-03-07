// SPDX-License-Identifier: ISC
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 */

#include <net/mac80211.h>
#include <linux/etherdevice.h>
#include "mac.h"
#include "core.h"
#include "debug.h"
#include "wmi.h"
#include "hw.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "testmode.h"

#define CHAN2G(_channel, _freq, _flags) { \
	.band                   = NL80211_BAND_2GHZ, \
	.hw_value               = (_channel), \
	.center_freq            = (_freq), \
	.flags                  = (_flags), \
	.max_antenna_gain       = 0, \
	.max_power              = 30, \
}

#define CHAN5G(_channel, _freq, _flags) { \
	.band                   = NL80211_BAND_5GHZ, \
	.hw_value               = (_channel), \
	.center_freq            = (_freq), \
	.flags                  = (_flags), \
	.max_antenna_gain       = 0, \
	.max_power              = 30, \
}

static const struct ieee80211_channel ath11k_2ghz_channels[] = {
	CHAN2G(1, 2412, 0),
	CHAN2G(2, 2417, 0),
	CHAN2G(3, 2422, 0),
	CHAN2G(4, 2427, 0),
	CHAN2G(5, 2432, 0),
	CHAN2G(6, 2437, 0),
	CHAN2G(7, 2442, 0),
	CHAN2G(8, 2447, 0),
	CHAN2G(9, 2452, 0),
	CHAN2G(10, 2457, 0),
	CHAN2G(11, 2462, 0),
	CHAN2G(12, 2467, 0),
	CHAN2G(13, 2472, 0),
	CHAN2G(14, 2484, 0),
};

static const struct ieee80211_channel ath11k_5ghz_channels[] = {
	CHAN5G(36, 5180, 0),
	CHAN5G(40, 5200, 0),
	CHAN5G(44, 5220, 0),
	CHAN5G(48, 5240, 0),
	CHAN5G(52, 5260, 0),
	CHAN5G(56, 5280, 0),
	CHAN5G(60, 5300, 0),
	CHAN5G(64, 5320, 0),
	CHAN5G(100, 5500, 0),
	CHAN5G(104, 5520, 0),
	CHAN5G(108, 5540, 0),
	CHAN5G(112, 5560, 0),
	CHAN5G(116, 5580, 0),
	CHAN5G(120, 5600, 0),
	CHAN5G(124, 5620, 0),
	CHAN5G(128, 5640, 0),
	CHAN5G(132, 5660, 0),
	CHAN5G(136, 5680, 0),
	CHAN5G(140, 5700, 0),
	CHAN5G(144, 5720, 0),
	CHAN5G(149, 5745, 0),
	CHAN5G(153, 5765, 0),
	CHAN5G(157, 5785, 0),
	CHAN5G(161, 5805, 0),
	CHAN5G(165, 5825, 0),
	CHAN5G(169, 5845, 0),
	CHAN5G(173, 5865, 0),
};

static struct ieee80211_rate ath11k_legacy_rates[] = {
	{ .bitrate = 10,
	  .hw_value = ATH11K_HW_RATE_CCK_LP_1M },
	{ .bitrate = 20,
	  .hw_value = ATH11K_HW_RATE_CCK_LP_2M,
	  .hw_value_short = ATH11K_HW_RATE_CCK_SP_2M,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 55,
	  .hw_value = ATH11K_HW_RATE_CCK_LP_5_5M,
	  .hw_value_short = ATH11K_HW_RATE_CCK_SP_5_5M,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 110,
	  .hw_value = ATH11K_HW_RATE_CCK_LP_11M,
	  .hw_value_short = ATH11K_HW_RATE_CCK_SP_11M,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },

	{ .bitrate = 60, .hw_value = ATH11K_HW_RATE_OFDM_6M },
	{ .bitrate = 90, .hw_value = ATH11K_HW_RATE_OFDM_9M },
	{ .bitrate = 120, .hw_value = ATH11K_HW_RATE_OFDM_12M },
	{ .bitrate = 180, .hw_value = ATH11K_HW_RATE_OFDM_18M },
	{ .bitrate = 240, .hw_value = ATH11K_HW_RATE_OFDM_24M },
	{ .bitrate = 360, .hw_value = ATH11K_HW_RATE_OFDM_36M },
	{ .bitrate = 480, .hw_value = ATH11K_HW_RATE_OFDM_48M },
	{ .bitrate = 540, .hw_value = ATH11K_HW_RATE_OFDM_54M },
};

#define ATH11K_MAC_FIRST_OFDM_RATE_IDX 4
#define ath11k_g_rates ath11k_legacy_rates
#define ath11k_g_rates_size (ARRAY_SIZE(ath11k_legacy_rates))
#define ath11k_a_rates (ath11k_legacy_rates + 4)
#define ath11k_a_rates_size (ARRAY_SIZE(ath11k_legacy_rates) - 4)

int ath11k_mac_hw_ratecode_to_legacy_rate(u8 hw_rc,
					    u8 preamble,
					    u8 *rateidx,
					    u16 *rate)
{
	/* As default, it is OFDM rates */
	int i = ATH11K_MAC_FIRST_OFDM_RATE_IDX;
	int max_rates_idx = ath11k_g_rates_size;

	if (preamble == WMI_RATE_PREAMBLE_CCK) {
		hw_rc &= ~ATH11k_HW_RATECODE_CCK_SHORT_PREAM_MASK;
		i = 0;
		max_rates_idx = ATH11K_MAC_FIRST_OFDM_RATE_IDX;
	}

	while (i < max_rates_idx) {
		if(hw_rc == ath11k_legacy_rates[i].hw_value) {
			*rateidx = i;
			*rate = ath11k_legacy_rates[i].bitrate;
			return 0;
		}
		i++;
	}

	return -EINVAL;
}

static int get_num_chains(u32 mask)
{
	int num_chains = 0;

	while (mask) {
		if (mask & BIT(0))
			num_chains++;
		mask >>= 1;
	}

	return num_chains;
}

static inline enum wmi_phy_mode
chan_to_phymode(const struct cfg80211_chan_def *chandef)
{
	enum wmi_phy_mode phymode = MODE_UNKNOWN;

	switch (chandef->chan->band) {
	case NL80211_BAND_2GHZ:
		switch (chandef->width) {
		case NL80211_CHAN_WIDTH_20_NOHT:
			if (chandef->chan->flags & IEEE80211_CHAN_NO_OFDM)
				phymode = MODE_11B;
			else
				phymode = MODE_11G;
			break;
		case NL80211_CHAN_WIDTH_20:
			phymode = MODE_11NG_HT20;
			break;
		case NL80211_CHAN_WIDTH_40:
			phymode = MODE_11NG_HT40;
			break;
		case NL80211_CHAN_WIDTH_5:
		case NL80211_CHAN_WIDTH_10:
		case NL80211_CHAN_WIDTH_80:
		case NL80211_CHAN_WIDTH_80P80:
		case NL80211_CHAN_WIDTH_160:
			phymode = MODE_UNKNOWN;
			break;
		}
		break;
	case NL80211_BAND_5GHZ:
		switch (chandef->width) {
		case NL80211_CHAN_WIDTH_20_NOHT:
			phymode = MODE_11A;
			break;
		case NL80211_CHAN_WIDTH_20:
			phymode = MODE_11NA_HT20;
			break;
		case NL80211_CHAN_WIDTH_40:
			phymode = MODE_11NA_HT40;
			break;
		case NL80211_CHAN_WIDTH_80:
			phymode = MODE_11AC_VHT80;
			break;
		case NL80211_CHAN_WIDTH_160:
			phymode = MODE_11AC_VHT160;
			break;
		case NL80211_CHAN_WIDTH_80P80:
			phymode = MODE_11AC_VHT80_80;
			break;
		case NL80211_CHAN_WIDTH_5:
		case NL80211_CHAN_WIDTH_10:
			phymode = MODE_UNKNOWN;
			break;
		}
		break;
	default:
		break;
	}

	WARN_ON(phymode == MODE_UNKNOWN);
	return phymode;
}

u8 ath11k_mac_bitrate_to_idx(const struct ieee80211_supported_band *sband,
			     u32 bitrate)
{
	int i;

	for (i = 0; i < sband->n_bitrates; i++)
		if (sband->bitrates[i].bitrate == bitrate)
			return i;

	return 0;
}

static u32
ath11k_mac_max_ht_nss(const u8 ht_mcs_mask[IEEE80211_HT_MCS_MASK_LEN])
{
	int nss;

	for (nss = IEEE80211_HT_MCS_MASK_LEN - 1; nss >= 0; nss--)
		if (ht_mcs_mask[nss])
			return nss + 1;

	return 1;
}

static u32
ath11k_mac_max_vht_nss(const u16 vht_mcs_mask[NL80211_VHT_NSS_MAX])
{
	int nss;

	for (nss = NL80211_VHT_NSS_MAX - 1; nss >= 0; nss--)
		if (vht_mcs_mask[nss])
			return nss + 1;

	return 1;
}

static u8 ath11k_parse_mpdudensity(u8 mpdudensity)
{
/* 802.11n D2.0 defined values for "Minimum MPDU Start Spacing":
 *   0 for no restriction
 *   1 for 1/4 us
 *   2 for 1/2 us
 *   3 for 1 us
 *   4 for 2 us
 *   5 for 4 us
 *   6 for 8 us
 *   7 for 16 us
 */
	switch (mpdudensity) {
	case 0:
		return 0;
	case 1:
	case 2:
	case 3:
	/* Our lower layer calculations limit our precision to
	 * 1 microsecond
	 */
		return 1;
	case 4:
		return 2;
	case 5:
		return 4;
	case 6:
		return 8;
	case 7:
		return 16;
	default:
		return 0;
	}
}

int ath11k_mac_vif_chan(struct ieee80211_vif *vif,
			struct cfg80211_chan_def *def)
{
	struct ieee80211_chanctx_conf *conf;

	rcu_read_lock();
	conf = rcu_dereference(vif->chanctx_conf);
	if (!conf) {
		rcu_read_unlock();
		return -ENOENT;
	}

	*def = conf->def;
	rcu_read_unlock();

	return 0;
}

static bool ath11k_mac_bitrate_is_cck(int bitrate)
{
	switch (bitrate) {
	case 10:
	case 20:
	case 55:
	case 110:
		return true;
	}

	return false;
}

u8 ath11k_mac_hw_rate_to_idx(const struct ieee80211_supported_band *sband,
			     u8 hw_rate, bool cck)
{
	const struct ieee80211_rate *rate;
	int i;

	for (i = 0; i < sband->n_bitrates; i++) {
		rate = &sband->bitrates[i];

		if (ath11k_mac_bitrate_is_cck(rate->bitrate) != cck)
			continue;

		if (rate->hw_value == hw_rate)
			return i;
		else if (rate->flags & IEEE80211_RATE_SHORT_PREAMBLE &&
			 rate->hw_value_short == hw_rate)
			return i;
	}

	return 0;
}

static u8 ath11k_mac_bitrate_to_rate(int bitrate)
{
	return DIV_ROUND_UP(bitrate, 5) |
	       (ath11k_mac_bitrate_is_cck(bitrate) ? BIT(7) : 0);
}

static void ath11k_get_arvif_iter(void *data, u8 *mac,
				  struct ieee80211_vif *vif)
{
	struct ath11k_vif_iter *arvif_iter = data;
	struct ath11k_vif *arvif = (void *)vif->drv_priv;

	if (arvif->vdev_id == arvif_iter->vdev_id)
		arvif_iter->arvif = arvif;
}

struct ath11k_vif *ath11k_get_arvif(struct ath11k *ar, u32 vdev_id)
{
	struct ath11k_vif_iter arvif_iter;
	u32 flags;

	memset(&arvif_iter, 0, sizeof(struct ath11k_vif_iter));
	arvif_iter.vdev_id = vdev_id;

	flags = IEEE80211_IFACE_ITER_RESUME_ALL;
	ieee80211_iterate_active_interfaces_atomic(ar->hw,
						   flags,
						   ath11k_get_arvif_iter,
						   &arvif_iter);
	if (!arvif_iter.arvif)
		return NULL;

	return arvif_iter.arvif;
}

struct ath11k *ath11k_get_ar_by_vdev_id(struct ath11k_base *ab, u32 vdev_id)
{
	int i;
	struct ath11k_pdev *pdev;
	struct ath11k_vif *arvif;

	if (!ab->mac_registered)
		return NULL;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = rcu_dereference(ab->pdevs_active[i]);
		if (pdev && pdev->ar) {
			arvif = ath11k_get_arvif(pdev->ar, vdev_id);
			if (arvif)
				return arvif->ar;
		}
	}

	return NULL;
}

struct ath11k *ath11k_get_ar_by_pdev_id(struct ath11k_base *ab, u32 pdev_id)
{
	int i;
	struct ath11k_pdev *pdev;

	if (!ab->mac_registered)
		return NULL;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = rcu_dereference(ab->pdevs_active[i]);

		if (pdev && pdev->pdev_id == pdev_id)
			return (pdev->ar ? pdev->ar : NULL);
	}

	return NULL;
}

int ath11k_pdev_caps_update(struct ath11k *ar)
{
	struct ath11k_base *ab = ar->ab;
	u8 pdev_idx = ar->pdev_idx;
	int status = 0;
	struct ath11k_hal_reg_capabilities_ext rcap = ar->hal_reg_cap;

	rcap.eeprom_reg_domain =
		ab->hal_reg_cap[pdev_idx].eeprom_reg_domain;
	rcap.eeprom_reg_domain_ext =
		ab->hal_reg_cap[pdev_idx].eeprom_reg_domain_ext;
	rcap.regcap1 = ab->hal_reg_cap[pdev_idx].regcap1;
	rcap.regcap2 = ab->hal_reg_cap[pdev_idx].regcap2;
	rcap.wireless_modes =
		ab->hal_reg_cap[pdev_idx].wireless_modes;
	rcap.low_5ghz_chan =
		ab->hal_reg_cap[pdev_idx].low_5ghz_chan;
	rcap.high_5ghz_chan =
		ab->hal_reg_cap[pdev_idx].high_5ghz_chan;
	rcap.low_2ghz_chan =
		ab->hal_reg_cap[pdev_idx].low_2ghz_chan;
	rcap.high_2ghz_chan =
		ab->hal_reg_cap[pdev_idx].high_2ghz_chan;

	ar->max_tx_power = ab->target_caps.hw_max_tx_power;

	/* FIXME Set min_tx_power to ab->target_caps.hw_min_tx_power.
	 * But since the received value in svcrdy is same as hw_max_tx_power,
	 * we can set ar->min_tx_power to 0 currently until
	 * this is fixed in firmware
	 */
	ar->min_tx_power = 0;

	ar->txpower_limit_2g = ar->max_tx_power;
	ar->txpower_limit_5g = ar->max_tx_power;
	ar->txpower_scale = WMI_HOST_TP_SCALE_MAX;

	return status;
}

static int ath11k_mac_txpower_recalc(struct ath11k *ar)
{
	struct ath11k_pdev *pdev = ar->pdev;
	struct ath11k_vif *arvif;
	int ret, txpower = -1;
	u32 param;

	lockdep_assert_held(&ar->conf_mutex);

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->txpower <= 0)
			continue;

		if (txpower == -1)
			txpower = arvif->txpower;
		else
			txpower = min(txpower, arvif->txpower);
	}

	if (txpower == -1)
		return 0;

	/* txpwr is set as 2 units per dBm in FW*/
	txpower = min_t(u32, max_t(u32, ar->min_tx_power, txpower),
			ar->max_tx_power) * 2;

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "txpower to set in hw %d\n",
		   txpower / 2);

	if ((pdev->cap.supported_bands & WMI_HOST_WLAN_2G_CAP) &&
	    ar->txpower_limit_2g != txpower) {
		param = WMI_PDEV_PARAM_TXPOWER_LIMIT2G;
		ret = ath11k_wmi_pdev_set_param(ar, param,
						txpower, ar->pdev->pdev_id);
		if (ret)
			goto fail;
		ar->txpower_limit_2g = txpower;
	}

	if ((pdev->cap.supported_bands & WMI_HOST_WLAN_5G_CAP) &&
	    ar->txpower_limit_5g != txpower) {
		param = WMI_PDEV_PARAM_TXPOWER_LIMIT5G;
		ret = ath11k_wmi_pdev_set_param(ar, param,
						txpower, ar->pdev->pdev_id);
		if (ret)
			goto fail;
		ar->txpower_limit_5g = txpower;
	}

	return 0;

fail:
	ath11k_warn(ar->ab, "failed to recalc txpower limit %d using pdev param %d: %d\n",
		    txpower / 2, param, ret);
	return ret;
}

static int ath11k_recalc_rtscts_prot(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	u32 vdev_param, rts_cts = 0;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	vdev_param = WMI_VDEV_PARAM_ENABLE_RTSCTS;

	/* Enable RTS/CTS protection for sw retries (when legacy stations
	 * are in BSS) or by default only for second rate series.
	 * TODO: Check if we need to enable CTS 2 Self in any case
	 */
	rts_cts = WMI_USE_RTS_CTS;

	if (arvif->num_legacy_stations > 0)
		rts_cts |= WMI_RTSCTS_ACROSS_SW_RETRIES << 4;
	else
		rts_cts |= WMI_RTSCTS_FOR_SECOND_RATESERIES << 4;

	/* Need not send duplicate param value to firmware */
	if (arvif->rtscts_prot_mode == rts_cts)
		return 0;

	arvif->rtscts_prot_mode = rts_cts;

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac vdev %d recalc rts/cts prot %d\n",
		   arvif->vdev_id, rts_cts);

	ret =  ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					     vdev_param, rts_cts);
	if (ret)
		ath11k_warn(ar->ab, "failed to recalculate rts/cts prot for vdev %d: %d\n",
			    arvif->vdev_id, ret);

	return ret;
}

static int ath11k_mac_set_kickout(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	u32 param;
	int ret;

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_STA_KICKOUT_TH,
					ATH11K_KICKOUT_THRESHOLD,
					ar->pdev->pdev_id);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set kickout threshold on vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	param = WMI_VDEV_PARAM_AP_KEEPALIVE_MIN_IDLE_INACTIVE_TIME_SECS;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param,
					    ATH11K_KEEPALIVE_MIN_IDLE);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set keepalive minimum idle time on vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	param = WMI_VDEV_PARAM_AP_KEEPALIVE_MAX_IDLE_INACTIVE_TIME_SECS;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param,
					    ATH11K_KEEPALIVE_MAX_IDLE);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set keepalive maximum idle time on vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	param = WMI_VDEV_PARAM_AP_KEEPALIVE_MAX_UNRESPONSIVE_TIME_SECS;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param,
					    ATH11K_KEEPALIVE_MAX_UNRESPONSIVE);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set keepalive maximum unresponsive time on vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	return 0;
}

static int ath11k_wait_for_peer_common(struct ath11k_base *ab, int vdev_id,
				       const u8 *addr, bool expect_mapped)
{
	int ret;

	ret = wait_event_timeout(ab->peer_mapping_wq, ({
				bool mapped;

				spin_lock_bh(&ab->data_lock);
				mapped = !!ath11k_peer_find(ab, vdev_id, addr);
				spin_unlock_bh(&ab->data_lock);

				mapped == expect_mapped;
				}), 3 * HZ);

	if (ret <= 0)
		return -ETIMEDOUT;

	return 0;
}

int ath11k_wait_for_peer_created(struct ath11k *ar, int vdev_id, const u8 *addr)
{
	return ath11k_wait_for_peer_common(ar->ab, vdev_id, addr, true);
}

int ath11k_wait_for_peer_deleted(struct ath11k *ar, int vdev_id, const u8 *addr)
{
	return ath11k_wait_for_peer_common(ar->ab, vdev_id, addr, false);
}

static int ath11k_peer_create(struct ath11k *ar, struct ath11k_vif *arvif,
			      struct ieee80211_sta *sta,
			      struct peer_create_params *param)
{
	struct ath11k_peer *peer;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	ret = ath11k_wmi_send_peer_create_cmd(ar, param);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send peer create vdev_id %d ret %d\n",
			    param->vdev_id, ret);
		return ret;
	}

	ret = ath11k_wait_for_peer_created(ar, param->vdev_id,
					   param->peer_addr);
	if (ret)
		return ret;

	spin_lock_bh(&ar->ab->data_lock);

	peer = ath11k_peer_find(ar->ab, param->vdev_id, param->peer_addr);
	if (!peer) {
		spin_unlock_bh(&ar->ab->data_lock);
		ath11k_warn(ar->ab, "failed to find peer %pM on vdev %i after creation\n",
			    param->peer_addr, param->vdev_id);
		ath11k_wmi_send_peer_delete_cmd(ar, param->peer_addr,
						param->vdev_id);
		return -ENOENT;
	}

	peer->sta = sta;
	arvif->ast_hash = peer->ast_hash;

	spin_unlock_bh(&ar->ab->data_lock);

	return 0;
}

static int ath11k_peer_delete(struct ath11k *ar, u32 vdev_id, u8 *addr)
{
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	ret = ath11k_wmi_send_peer_delete_cmd(ar, addr, vdev_id);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to delete peer vdev_id %d addr %pM ret %d\n",
			    vdev_id, addr, ret);
		return ret;
	}

	ret = ath11k_wait_for_peer_deleted(ar, vdev_id, addr);
	if (ret)
		return ret;

	return 0;
}

static void ath11k_peer_cleanup(struct ath11k *ar, u32 vdev_id)
{
	struct ath11k_peer *peer, *tmp;
	struct ath11k_base *ab = ar->ab;

	lockdep_assert_held(&ar->conf_mutex);

	spin_lock_bh(&ab->data_lock);
	list_for_each_entry_safe(peer, tmp, &ab->peers, list) {
		if (peer->vdev_id != vdev_id)
			continue;

		ath11k_warn(ab, "removing stale peer %pM from vdev_id %d\n",
			    peer->addr, vdev_id);

		list_del(&peer->list);
		kfree(peer);
	}
	spin_unlock_bh(&ab->data_lock);
}

static int ath11k_config(struct ieee80211_hw *hw, u32 changed)
{
	struct ath11k *ar = hw->priv;
	int ret = 0;

	mutex_lock(&ar->conf_mutex);

	/* TODO: Handle configuration changes as appropriate */

	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static int ath11k_mac_setup_bcn_tmpl(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;
	struct ieee80211_hw *hw = ar->hw;
	struct ieee80211_vif *vif = arvif->vif;
	struct ieee80211_mutable_offsets offs = {};
	struct sk_buff *bcn;
	int ret;

	if (arvif->vdev_type != WMI_VDEV_TYPE_AP)
		return 0;

	bcn = ieee80211_beacon_get_template(hw, vif, &offs);
	if (!bcn) {
		ath11k_warn(ab, "failed to get beacon template from mac80211\n");
		return -EPERM;
	}

	ret = ath11k_wmi_bcn_tmpl(ar, arvif->vdev_id, &offs, bcn);

	kfree_skb(bcn);

	if (ret)
		ath11k_warn(ab, "failed to submit beacon template command: %d\n",
			    ret);

	return ret;
}

static int ath11k_mac_setup_prb_tmpl(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	struct ieee80211_hw *hw = ar->hw;
	struct ieee80211_vif *vif = arvif->vif;
	struct sk_buff *prb;
	int ret;

	/* Interface validation is part of the below function, need not
	 * check here.
	 */
	prb = ieee80211_proberesp_get(hw, vif);
	if (!prb) {
		ath11k_warn(ar->ab, "failed to get probe resp template from mac80211\n");
		return -EPERM;
	}

	ret = ath11k_wmi_prb_tmpl(ar, arvif->vdev_id, prb);
	kfree_skb(prb);

	if (ret) {
		ath11k_warn(ar->ab, "failed to submit probe resp template command: %d\n",
			    ret);
	}

	return ret;
}

static void ath11k_control_beaconing(struct ath11k_vif *arvif,
				     struct ieee80211_bss_conf *info)
{
	struct ath11k *ar = arvif->ar;
	int ret = 0;

	lockdep_assert_held(&arvif->ar->conf_mutex);

	if (!info->enable_beacon) {
		ret = ath11k_wmi_vdev_down(ar, arvif->vdev_id);
		if (ret)
			ath11k_warn(ar->ab, "failed to down vdev_id %i: %d\n",
				    arvif->vdev_id, ret);

		arvif->is_up = false;
		return;
	}

	/* Install the beacon template to the FW */
	ret = ath11k_mac_setup_bcn_tmpl(arvif);
	if (ret) {
		ath11k_warn(ar->ab, "failed to update bcn tmpl during vdev up: %d\n",
			    ret);
		return;
	}

	arvif->tx_seq_no = 0x1000;

	arvif->aid = 0;

	ether_addr_copy(arvif->bssid, info->bssid);

	ret = ath11k_wmi_vdev_up(arvif->ar, arvif->vdev_id, arvif->aid,
				 arvif->bssid);
	if (ret) {
		ath11k_warn(ar->ab, "failed to bring up vdev %d: %i\n",
			    arvif->vdev_id, ret);
		return;
	}

	arvif->is_up = true;

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac vdev %d up\n", arvif->vdev_id);
}

static void ath11k_peer_assoc_h_basic(struct ath11k *ar,
				      struct ieee80211_vif *vif,
				      struct ieee80211_sta *sta,
				      struct peer_assoc_params *arg)
{
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	u32 aid;

	lockdep_assert_held(&ar->conf_mutex);

	if (vif->type == NL80211_IFTYPE_STATION)
		aid = vif->bss_conf.aid;
	else
		aid = sta->aid;

	ether_addr_copy(arg->peer_mac, sta->addr);
	arg->vdev_id = arvif->vdev_id;
	arg->peer_associd = aid;
	arg->auth_flag = true;
	/* TODO: STA WAR in ath10k for listen interval required? */
	arg->peer_listen_intval = ar->hw->conf.listen_interval;
	arg->peer_nss = 1;
	arg->peer_caps = vif->bss_conf.assoc_capability;
}

static void ath11k_peer_assoc_h_crypto(struct ath11k *ar,
				       struct ieee80211_vif *vif,
				       struct ieee80211_sta *sta,
				       struct peer_assoc_params *arg)
{
	struct ieee80211_bss_conf *info = &vif->bss_conf;
	struct cfg80211_chan_def def;
	struct cfg80211_bss *bss;
	const u8 *rsnie = NULL;
	const u8 *wpaie = NULL;

	lockdep_assert_held(&ar->conf_mutex);

	if (WARN_ON(ath11k_mac_vif_chan(vif, &def)))
		return;

	bss = cfg80211_get_bss(ar->hw->wiphy, def.chan, info->bssid, NULL, 0,
			       IEEE80211_BSS_TYPE_ANY, IEEE80211_PRIVACY_ANY);
	if (bss) {
		const struct cfg80211_bss_ies *ies;

		rcu_read_lock();
		rsnie = ieee80211_bss_get_ie(bss, WLAN_EID_RSN);

		ies = rcu_dereference(bss->ies);

		wpaie = cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT,
						WLAN_OUI_TYPE_MICROSOFT_WPA,
						ies->data,
						ies->len);
		rcu_read_unlock();
		cfg80211_put_bss(ar->hw->wiphy, bss);
	}

	/* FIXME: base on RSN IE/WPA IE is a correct idea? */
	if (rsnie || wpaie) {
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "%s: rsn ie found\n", __func__);
		arg->need_ptk_4_way = true;
	}

	if (wpaie) {
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "%s: wpa ie found\n", __func__);
		arg->need_gtk_2_way = true;
	}

	if (sta->mfp) {
		/* TODO: Need to check if FW supports PMF? */
		arg->is_pmf_enabled = true;
	}

	/* TODO: safe_mode_enabled (bypass 4-way handshake) flag req? */
}

static void ath11k_peer_assoc_h_rates(struct ath11k *ar,
				      struct ieee80211_vif *vif,
				      struct ieee80211_sta *sta,
				      struct peer_assoc_params *arg)
{
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	struct wmi_rate_set_arg *rateset = &arg->peer_legacy_rates;
	struct cfg80211_chan_def def;
	const struct ieee80211_supported_band *sband;
	const struct ieee80211_rate *rates;
	enum nl80211_band band;
	u32 ratemask;
	u8 rate;
	int i;

	lockdep_assert_held(&ar->conf_mutex);

	if (WARN_ON(ath11k_mac_vif_chan(vif, &def)))
		return;

	band = def.chan->band;
	sband = ar->hw->wiphy->bands[band];
	ratemask = sta->supp_rates[band];
	ratemask &= arvif->bitrate_mask.control[band].legacy;
	rates = sband->bitrates;

	rateset->num_rates = 0;

	for (i = 0; i < 32; i++, ratemask >>= 1, rates++) {
		if (!(ratemask & 1))
			continue;

		rate = ath11k_mac_bitrate_to_rate(rates->bitrate);
		rateset->rates[rateset->num_rates] = rate;
		rateset->num_rates++;
	}
}

static bool
ath11k_peer_assoc_h_ht_masked(const u8 ht_mcs_mask[IEEE80211_HT_MCS_MASK_LEN])
{
	int nss;

	for (nss = 0; nss < IEEE80211_HT_MCS_MASK_LEN; nss++)
		if (ht_mcs_mask[nss])
			return false;

	return true;
}

static bool
ath11k_peer_assoc_h_vht_masked(const u16 vht_mcs_mask[NL80211_VHT_NSS_MAX])
{
	int nss;

	for (nss = 0; nss < NL80211_VHT_NSS_MAX; nss++)
		if (vht_mcs_mask[nss])
			return false;

	return true;
}

static void ath11k_peer_assoc_h_ht(struct ath11k *ar,
				   struct ieee80211_vif *vif,
				   struct ieee80211_sta *sta,
				   struct peer_assoc_params *arg)
{
	const struct ieee80211_sta_ht_cap *ht_cap = &sta->ht_cap;
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	const u8 *ht_mcs_mask;
	int i, n;
	u8 max_nss;
	u32 stbc;

	lockdep_assert_held(&ar->conf_mutex);

	if (WARN_ON(ath11k_mac_vif_chan(vif, &def)))
		return;

	if (!ht_cap->ht_supported)
		return;

	band = def.chan->band;
	ht_mcs_mask = arvif->bitrate_mask.control[band].ht_mcs;

	if (ath11k_peer_assoc_h_ht_masked(ht_mcs_mask))
		return;

	arg->ht_flag = true;

	arg->peer_max_mpdu = (1 << (IEEE80211_HT_MAX_AMPDU_FACTOR +
				    ht_cap->ampdu_factor)) - 1;

	arg->peer_mpdu_density =
		ath11k_parse_mpdudensity(ht_cap->ampdu_density);

	arg->peer_ht_caps = ht_cap->cap;
	arg->peer_rate_caps |= WMI_HOST_RC_HT_FLAG;

	if (ht_cap->cap & IEEE80211_HT_CAP_LDPC_CODING)
		arg->ldpc_flag = true;

	if (sta->bandwidth >= IEEE80211_STA_RX_BW_40) {
		arg->bw_40 = true;
		arg->peer_rate_caps |= WMI_HOST_RC_CW40_FLAG;
	}

	if (arvif->bitrate_mask.control[band].gi != NL80211_TXRATE_FORCE_LGI) {
		if (ht_cap->cap & (IEEE80211_HT_CAP_SGI_20 |
		    IEEE80211_HT_CAP_SGI_40))
			arg->peer_rate_caps |= WMI_HOST_RC_SGI_FLAG;
	}

	if (ht_cap->cap & IEEE80211_HT_CAP_TX_STBC) {
		arg->peer_rate_caps |= WMI_HOST_RC_TX_STBC_FLAG;
		arg->stbc_flag = true;
	}

	if (ht_cap->cap & IEEE80211_HT_CAP_RX_STBC) {
		stbc = ht_cap->cap & IEEE80211_HT_CAP_RX_STBC;
		stbc = stbc >> IEEE80211_HT_CAP_RX_STBC_SHIFT;
		stbc = stbc << WMI_HOST_RC_RX_STBC_FLAG_S;
		arg->peer_rate_caps |= stbc;
		arg->stbc_flag = true;
	}

	if (ht_cap->mcs.rx_mask[1] && ht_cap->mcs.rx_mask[2])
		arg->peer_rate_caps |= WMI_HOST_RC_TS_FLAG;
	else if (ht_cap->mcs.rx_mask[1])
		arg->peer_rate_caps |= WMI_HOST_RC_DS_FLAG;

	for (i = 0, n = 0, max_nss = 0; i < IEEE80211_HT_MCS_MASK_LEN * 8; i++)
		if ((ht_cap->mcs.rx_mask[i / 8] & BIT(i % 8)) &&
		    (ht_mcs_mask[i / 8] & BIT(i % 8))) {
			max_nss = (i / 8) + 1;
			arg->peer_ht_rates.rates[n++] = i;
		}

	/* This is a workaround for HT-enabled STAs which break the spec
	 * and have no HT capabilities RX mask (no HT RX MCS map).
	 *
	 * As per spec, in section 20.3.5 Modulation and coding scheme (MCS),
	 * MCS 0 through 7 are mandatory in 20MHz with 800 ns GI at all STAs.
	 *
	 * Firmware asserts if such situation occurs.
	 */
	if (n == 0) {
		arg->peer_ht_rates.num_rates = 8;
		for (i = 0; i < arg->peer_ht_rates.num_rates; i++)
			arg->peer_ht_rates.rates[i] = i;
	} else {
		arg->peer_ht_rates.num_rates = n;
		arg->peer_nss = min(sta->rx_nss, max_nss);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac ht peer %pM mcs cnt %d nss %d\n",
		   arg->peer_mac,
		   arg->peer_ht_rates.num_rates,
		   arg->peer_nss);
}

static int ath11k_mac_get_max_vht_mcs_map(u16 mcs_map, int nss)
{
	switch ((mcs_map >> (2 * nss)) & 0x3) {
	case IEEE80211_VHT_MCS_SUPPORT_0_7: return BIT(8) - 1;
	case IEEE80211_VHT_MCS_SUPPORT_0_8: return BIT(9) - 1;
	case IEEE80211_VHT_MCS_SUPPORT_0_9: return BIT(10) - 1;
	}
	return 0;
}

static u16
ath11k_peer_assoc_h_vht_limit(u16 tx_mcs_set,
			      const u16 vht_mcs_limit[NL80211_VHT_NSS_MAX])
{
	int idx_limit;
	int nss;
	u16 mcs_map;
	u16 mcs;

	for (nss = 0; nss < NL80211_VHT_NSS_MAX; nss++) {
		mcs_map = ath11k_mac_get_max_vht_mcs_map(tx_mcs_set, nss) &
			  vht_mcs_limit[nss];

		if (mcs_map)
			idx_limit = fls(mcs_map) - 1;
		else
			idx_limit = -1;

		switch (idx_limit) {
		case 0: /* fall through */
		case 1: /* fall through */
		case 2: /* fall through */
		case 3: /* fall through */
		case 4: /* fall through */
		case 5: /* fall through */
		case 6: /* fall through */
		default:
			/* see ath10k_mac_can_set_bitrate_mask() */
			WARN_ON(1);
			/* fall through */
		case -1:
			mcs = IEEE80211_VHT_MCS_NOT_SUPPORTED;
			break;
		case 7:
			mcs = IEEE80211_VHT_MCS_SUPPORT_0_7;
			break;
		case 8:
			mcs = IEEE80211_VHT_MCS_SUPPORT_0_8;
			break;
		case 9:
			mcs = IEEE80211_VHT_MCS_SUPPORT_0_9;
			break;
		}

		tx_mcs_set &= ~(0x3 << (nss * 2));
		tx_mcs_set |= mcs << (nss * 2);
	}

	return tx_mcs_set;
}

static void ath11k_peer_assoc_h_vht(struct ath11k *ar,
				    struct ieee80211_vif *vif,
				    struct ieee80211_sta *sta,
				    struct peer_assoc_params *arg)
{
	const struct ieee80211_sta_vht_cap *vht_cap = &sta->vht_cap;
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	const u16 *vht_mcs_mask;
	u8 ampdu_factor;
	u8 max_nss, vht_mcs;
	int i;

	if (WARN_ON(ath11k_mac_vif_chan(vif, &def)))
		return;

	if (!vht_cap->vht_supported)
		return;

	band = def.chan->band;
	vht_mcs_mask = arvif->bitrate_mask.control[band].vht_mcs;

	if (ath11k_peer_assoc_h_vht_masked(vht_mcs_mask))
		return;

	arg->vht_flag = true;

	/* TODO: similar flags required? */
	arg->vht_capable = true;

	if (def.chan->band == NL80211_BAND_2GHZ)
		arg->vht_ng_flag = true;

	arg->peer_vht_caps = vht_cap->cap;

	ampdu_factor = (vht_cap->cap &
			IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK) >>
		       IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT;

	/* Workaround: Some Netgear/Linksys 11ac APs set Rx A-MPDU factor to
	 * zero in VHT IE. Using it would result in degraded throughput.
	 * arg->peer_max_mpdu at this point contains HT max_mpdu so keep
	 * it if VHT max_mpdu is smaller.
	 */
	arg->peer_max_mpdu = max(arg->peer_max_mpdu,
				 (1U << (IEEE80211_HT_MAX_AMPDU_FACTOR +
					ampdu_factor)) - 1);

	if (sta->bandwidth == IEEE80211_STA_RX_BW_80)
		arg->bw_80 = true;

	if (sta->bandwidth == IEEE80211_STA_RX_BW_160)
		arg->bw_160 = true;

	/* Calculate peer NSS capability from VHT capabilities if STA
	 * supports VHT.
	 */
	for (i = 0, max_nss = 0, vht_mcs = 0; i < NL80211_VHT_NSS_MAX; i++) {
		vht_mcs = __le16_to_cpu(vht_cap->vht_mcs.rx_mcs_map) >>
			  (2 * i) & 3;

		if (vht_mcs != IEEE80211_VHT_MCS_NOT_SUPPORTED &&
		    vht_mcs_mask[i])
			max_nss = i + 1;
	}
	arg->peer_nss = min(sta->rx_nss, max_nss);
	arg->rx_max_rate = __le16_to_cpu(vht_cap->vht_mcs.rx_highest);
	arg->rx_mcs_set = __le16_to_cpu(vht_cap->vht_mcs.rx_mcs_map);
	arg->tx_max_rate = __le16_to_cpu(vht_cap->vht_mcs.tx_highest);
	arg->tx_mcs_set = ath11k_peer_assoc_h_vht_limit(
		__le16_to_cpu(vht_cap->vht_mcs.tx_mcs_map), vht_mcs_mask);

	/* TODO:  Check */
	arg->tx_max_mcs_nss = 0xFF;

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac vht peer %pM max_mpdu %d flags 0x%x\n",
		   sta->addr, arg->peer_max_mpdu, arg->peer_flags);

	/* TODO: rxnss_override */
}

static void ath11k_peer_assoc_h_he(struct ath11k *ar,
				   struct ieee80211_vif *vif,
				   struct ieee80211_sta *sta,
				   struct peer_assoc_params *arg)
{
	/* TODO: Implementation */
}

static void ath11k_peer_assoc_h_smps(struct ieee80211_sta *sta,
				     struct peer_assoc_params *arg)
{
	const struct ieee80211_sta_ht_cap *ht_cap = &sta->ht_cap;
	int smps;

	if (!ht_cap->ht_supported)
		return;

	smps = ht_cap->cap & IEEE80211_HT_CAP_SM_PS;
	smps >>= IEEE80211_HT_CAP_SM_PS_SHIFT;

	switch (smps) {
	case WLAN_HT_CAP_SM_PS_STATIC:
		arg->static_mimops_flag = true;
		break;
	case WLAN_HT_CAP_SM_PS_DYNAMIC:
		arg->dynamic_mimops_flag = true;
		break;
	case WLAN_HT_CAP_SM_PS_DISABLED:
		arg->spatial_mux_flag = true;
		break;
	default:
		break;
	}
}

static void ath11k_peer_assoc_h_qos(struct ath11k *ar,
				    struct ieee80211_vif *vif,
				    struct ieee80211_sta *sta,
				    struct peer_assoc_params *arg)
{
	struct ath11k_vif *arvif = (void *)vif->drv_priv;

	switch (arvif->vdev_type) {
	case WMI_VDEV_TYPE_AP:
		if (sta->wme) {
			/* TODO: Check WME vs QoS */
			arg->is_wme_set = true;
			arg->qos_flag = true;
		}

		if (sta->wme && sta->uapsd_queues) {
			/* TODO: Check WME vs QoS */
			arg->is_wme_set = true;
			arg->apsd_flag = true;
			arg->peer_rate_caps |= WMI_HOST_RC_UAPSD_FLAG;
		}
		break;
	case WMI_VDEV_TYPE_STA:
		if (sta->wme)
			arg->qos_flag = true;
		break;
	default:
		break;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac peer %pM qos %d\n",
		   sta->addr, arg->qos_flag);
}

static bool ath11k_mac_sta_has_ofdm_only(struct ieee80211_sta *sta)
{
	return sta->supp_rates[NL80211_BAND_2GHZ] >>
	       ATH11K_MAC_FIRST_OFDM_RATE_IDX;
}

static enum wmi_phy_mode ath11k_mac_get_phymode_vht(struct ath11k *ar,
						    struct ieee80211_sta *sta)
{
	if (sta->bandwidth == IEEE80211_STA_RX_BW_160) {
		switch (sta->vht_cap.cap &
			IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_MASK) {
		case IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ:
			return MODE_11AC_VHT160;
		case IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ:
			return MODE_11AC_VHT80_80;
		default:
			/* not sure if this is a valid case? */
			return MODE_11AC_VHT160;
		}
	}

	if (sta->bandwidth == IEEE80211_STA_RX_BW_80)
		return MODE_11AC_VHT80;

	if (sta->bandwidth == IEEE80211_STA_RX_BW_40)
		return MODE_11AC_VHT40;

	if (sta->bandwidth == IEEE80211_STA_RX_BW_20)
		return MODE_11AC_VHT20;

	return MODE_UNKNOWN;
}

static void ath11k_peer_assoc_h_phymode(struct ath11k *ar,
					struct ieee80211_vif *vif,
					struct ieee80211_sta *sta,
					struct peer_assoc_params *arg)
{
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	const u8 *ht_mcs_mask;
	const u16 *vht_mcs_mask;
	enum wmi_phy_mode phymode = MODE_UNKNOWN;

	if (WARN_ON(ath11k_mac_vif_chan(vif, &def)))
		return;

	band = def.chan->band;
	ht_mcs_mask = arvif->bitrate_mask.control[band].ht_mcs;
	vht_mcs_mask = arvif->bitrate_mask.control[band].vht_mcs;

	switch (band) {
	case NL80211_BAND_2GHZ:
		if (sta->vht_cap.vht_supported &&
		    !ath11k_peer_assoc_h_vht_masked(vht_mcs_mask)) {
			if (sta->bandwidth == IEEE80211_STA_RX_BW_40)
				phymode = MODE_11AC_VHT40;
			else
				phymode = MODE_11AC_VHT20;
		} else if (sta->ht_cap.ht_supported &&
			   !ath11k_peer_assoc_h_ht_masked(ht_mcs_mask)) {
			if (sta->bandwidth == IEEE80211_STA_RX_BW_40)
				phymode = MODE_11NG_HT40;
			else
				phymode = MODE_11NG_HT20;
		} else if (ath11k_mac_sta_has_ofdm_only(sta)) {
			phymode = MODE_11G;
		} else {
			phymode = MODE_11B;
		}
		/* TODO: HE */

		break;
	case NL80211_BAND_5GHZ:
		/* Check VHT first */
		if (sta->vht_cap.vht_supported &&
		    !ath11k_peer_assoc_h_vht_masked(vht_mcs_mask)) {
			phymode = ath11k_mac_get_phymode_vht(ar, sta);
		} else if (sta->ht_cap.ht_supported &&
			   !ath11k_peer_assoc_h_ht_masked(ht_mcs_mask)) {
			if (sta->bandwidth >= IEEE80211_STA_RX_BW_40)
				phymode = MODE_11NA_HT40;
			else
				phymode = MODE_11NA_HT20;
		} else {
			phymode = MODE_11A;
		}
		/* TODO: HE Phymode */
		break;
	default:
		break;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac peer %pM phymode %s\n",
		   sta->addr, ath11k_wmi_phymode_str(phymode));

	arg->peer_phymode = phymode;
	WARN_ON(phymode == MODE_UNKNOWN);
}

static void ath11k_peer_assoc_prepare(struct ath11k *ar,
				      struct ieee80211_vif *vif,
				      struct ieee80211_sta *sta,
				      struct peer_assoc_params *arg,
				      bool reassoc)
{
	lockdep_assert_held(&ar->conf_mutex);

	memset(arg, 0, sizeof(*arg));

	reinit_completion(&ar->peer_assoc_done);

	arg->peer_new_assoc = !reassoc;
	ath11k_peer_assoc_h_basic(ar, vif, sta, arg);
	ath11k_peer_assoc_h_crypto(ar, vif, sta, arg);
	ath11k_peer_assoc_h_rates(ar, vif, sta, arg);
	ath11k_peer_assoc_h_ht(ar, vif, sta, arg);
	ath11k_peer_assoc_h_vht(ar, vif, sta, arg);
	ath11k_peer_assoc_h_he(ar, vif, sta, arg);
	ath11k_peer_assoc_h_qos(ar, vif, sta, arg);
	ath11k_peer_assoc_h_phymode(ar, vif, sta, arg);
	ath11k_peer_assoc_h_smps(sta, arg);

	/* TODO: amsdu_disable req? */
}

static void ath11k_bss_assoc(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif,
			     struct ieee80211_bss_conf *bss_conf)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	struct peer_assoc_params peer_arg;
	struct ieee80211_sta *ap_sta;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac vdev %i assoc bssid %pM aid %d\n",
		   arvif->vdev_id, arvif->bssid, arvif->aid);

	rcu_read_lock();

	ap_sta = ieee80211_find_sta(vif, bss_conf->bssid);
	if (!ap_sta) {
		ath11k_warn(ar->ab, "failed to find station entry for bss %pM vdev %i\n",
			    bss_conf->bssid, arvif->vdev_id);
		rcu_read_unlock();
		return;
	}

	ath11k_peer_assoc_prepare(ar, vif, ap_sta, &peer_arg, false);

	rcu_read_unlock();

	ret = ath11k_wmi_send_peer_assoc_cmd(ar, &peer_arg);
	if (ret) {
		ath11k_warn(ar->ab, "failed to run peer assoc for %pM vdev %i: %d\n",
			    bss_conf->bssid, arvif->vdev_id, ret);
		return;
	}

	if (!wait_for_completion_timeout(&ar->peer_assoc_done, 1 * HZ)) {
		ath11k_warn(ar->ab, "failed to get peer assoc conf event for %pM vdev %i\n",
			    bss_conf->bssid, arvif->vdev_id);
		return;
	}

	WARN_ON(arvif->is_up);

	arvif->aid = bss_conf->aid;
	ether_addr_copy(arvif->bssid, bss_conf->bssid);

	ret = ath11k_wmi_vdev_up(ar, arvif->vdev_id, arvif->aid, arvif->bssid);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set vdev %d up: %d\n",
			    arvif->vdev_id, ret);
		return;
	}

	arvif->is_up = true;

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC,
		   "mac vdev %d up (associated) bssid %pM aid %d\n",
		   arvif->vdev_id, bss_conf->bssid, bss_conf->aid);

	/* Authorize BSS Peer */
	ret = ath11k_wmi_set_peer_param(ar, arvif->bssid,
					arvif->vdev_id,
					WMI_PEER_AUTHORIZE,
					1);
	if (ret)
		ath11k_warn(ar->ab, "Unable to authorize BSS peer: %d\n", ret);
}

static void ath11k_bss_disassoc(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac vdev %i disassoc bssid %pM\n",
		   arvif->vdev_id, arvif->bssid);

	ret = ath11k_wmi_vdev_down(ar, arvif->vdev_id);
	if (ret)
		ath11k_warn(ar->ab, "failed to down vdev %i: %d\n",
			    arvif->vdev_id, ret);

	arvif->def_wep_key_index = -1;

	arvif->is_up = false;

	/* TODO: cancel connection_loss_work */
}

static void ath11k_bss_info_changed(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct ieee80211_bss_conf *info,
				    u32 changed)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	u32 param_id, param_value;
	int ret = 0;

	mutex_lock(&ar->conf_mutex);

	if (changed & BSS_CHANGED_BEACON_INT) {
		arvif->beacon_interval = info->beacon_int;

		param_id = WMI_VDEV_PARAM_BEACON_INTERVAL;
		ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    param_id,
						    arvif->beacon_interval);
		if (ret)
			ath11k_warn(ar->ab, "Failed to set beacon interval for VDEV: %d\n",
				    arvif->vdev_id);
		else
			ath11k_info(ar->ab,
				    "Beacon interval: %d set for VDEV: %d\n",
				    arvif->beacon_interval, arvif->vdev_id);
	}

	if (changed & BSS_CHANGED_BEACON) {
		param_id = WMI_PDEV_PARAM_BEACON_TX_MODE;
		param_value = WMI_BEACON_STAGGERED_MODE;
		ret = ath11k_wmi_pdev_set_param(ar, param_id,
						param_value, ar->pdev->pdev_id);
		if (ret)
			ath11k_warn(ar->ab, "Failed to set beacon mode for VDEV: %d\n",
				    arvif->vdev_id);
		else
			ath11k_info(ar->ab,
				    "Set staggered beacon mode for VDEV: %d\n",
				    arvif->vdev_id);

		ret = ath11k_mac_setup_bcn_tmpl(arvif);
		if (ret)
			ath11k_warn(ar->ab, "failed to update bcn template: %d\n",
				    ret);
	}

	if (changed & BSS_CHANGED_AP_PROBE_RESP) {
		ret = ath11k_mac_setup_prb_tmpl(arvif);
		if (ret)
			ath11k_warn(ar->ab, "failed to setup probe resp template on vdev %i: %d\n",
				    arvif->vdev_id, ret);
	}

	if (changed & BSS_CHANGED_SSID &&
	    vif->type == NL80211_IFTYPE_AP) {
		arvif->u.ap.ssid_len = info->ssid_len;
		if (info->ssid_len)
			memcpy(arvif->u.ap.ssid, info->ssid, info->ssid_len);
		arvif->u.ap.hidden_ssid = info->hidden_ssid;
	}

	if (changed & BSS_CHANGED_BSSID && !is_zero_ether_addr(info->bssid))
		ether_addr_copy(arvif->bssid, info->bssid);

	if (changed & BSS_CHANGED_BEACON_ENABLED)
		ath11k_control_beaconing(arvif, info);

	if (changed & BSS_CHANGED_ERP_CTS_PROT) {
		u32 cts_prot;
		cts_prot = !!(info->use_cts_prot);
		param_id = WMI_VDEV_PARAM_PROTECTION_MODE;

		if (arvif->is_started) {
			ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
							    param_id, cts_prot);
			if (ret)
				ath11k_warn(ar->ab, "Failed to set CTS prot for VDEV: %d\n",
					    arvif->vdev_id);
			else
				ath11k_info(ar->ab, "Set CTS prot: %d for VDEV: %d\n",
					    cts_prot, arvif->vdev_id);
		} else {
			ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "defer protection mode setup, vdev is not ready yet\n");
		}
	}

	if (changed & BSS_CHANGED_ERP_SLOT) {
		u32 slottime;

		if (info->use_short_slot)
			slottime = WMI_VDEV_SLOT_TIME_SHORT; /* 9us */

		else
			slottime = WMI_VDEV_SLOT_TIME_LONG; /* 20us */

		param_id = WMI_VDEV_PARAM_SLOT_TIME;
		ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    param_id, slottime);
		if (ret)
			ath11k_warn(ar->ab, "Failed to set erp slot for VDEV: %d\n",
				    arvif->vdev_id);
		else
			ath11k_info(ar->ab,
				    "Set slottime: %d for VDEV: %d\n",
				    slottime, arvif->vdev_id);
	}

	if (changed & BSS_CHANGED_ERP_PREAMBLE) {
		u32 preamble;

		if (info->use_short_preamble)
			preamble = WMI_VDEV_PREAMBLE_SHORT;
		else
			preamble = WMI_VDEV_PREAMBLE_LONG;

		param_id = WMI_VDEV_PARAM_PREAMBLE;
		ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    param_id, preamble);
		if (ret)
			ath11k_warn(ar->ab, "Failed to set preamble for VDEV: %d\n",
				    arvif->vdev_id);
		else
			ath11k_info(ar->ab,
				    "Set preamble: %d for VDEV: %d\n",
				    preamble, arvif->vdev_id);
	}

	if (changed & BSS_CHANGED_ASSOC) {
		if (info->assoc)
			ath11k_bss_assoc(hw, vif, info);
		else
			ath11k_bss_disassoc(hw, vif);
	}

	if (changed & BSS_CHANGED_TXPOWER) {
		ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac vdev_id %i txpower %d\n",
			   arvif->vdev_id, info->txpower);

		arvif->txpower = info->txpower;
		ath11k_mac_txpower_recalc(ar);
	}

	mutex_unlock(&ar->conf_mutex);
}

/************/
/* Scanning */
/************/

void __ath11k_scan_finish(struct ath11k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH11K_SCAN_IDLE:
		break;
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		if (!ar->scan.is_roc) {
			struct cfg80211_scan_info info = {
				.aborted = (ar->scan.state ==
					    ATH11K_SCAN_ABORTING),
			};

			ieee80211_scan_completed(ar->hw, &info);
		} else if (ar->scan.roc_notify) {
			ieee80211_remain_on_channel_expired(ar->hw);
		}
		/* fall through */
	case ATH11K_SCAN_STARTING:
		ar->scan.state = ATH11K_SCAN_IDLE;
		ar->scan_channel = NULL;
		ar->scan.roc_freq = 0;
		cancel_delayed_work(&ar->scan.timeout);
		complete(&ar->scan.completed);
		break;
	}
}

void ath11k_scan_finish(struct ath11k *ar)
{
	spin_lock_bh(&ar->data_lock);
	__ath11k_scan_finish(ar);
	spin_unlock_bh(&ar->data_lock);
}

static int ath11k_scan_stop(struct ath11k *ar)
{
	struct scan_cancel_param arg = {
		.req_type = WMI_SCAN_STOP_ONE,
		.scan_id = ATH11K_SCAN_ID,
	};
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	/* TODO: Fill other STOP Params */
	arg.pdev_id = ar->pdev->pdev_id;

	ret = ath11k_wmi_send_scan_stop_cmd(ar, &arg);
	if (ret) {
		ath11k_warn(ar->ab, "failed to stop wmi scan: %d\n", ret);
		goto out;
	}

	ret = wait_for_completion_timeout(&ar->scan.completed, 3 * HZ);
	if (ret == 0) {
		ath11k_warn(ar->ab,
			    "failed to receive scan abort comple: timed out\n");
		ret = -ETIMEDOUT;
	} else if (ret > 0) {
		ret = 0;
	}

out:
	/* Scan state should be updated upon scan completion but in case
	 * firmware fails to deliver the event (for whatever reason) it is
	 * desired to clean up scan state anyway. Firmware may have just
	 * dropped the scan completion event delivery due to transport pipe
	 * being overflown with data and/or it can recover on its own before
	 * next scan request is submitted.
	 */
	spin_lock_bh(&ar->data_lock);
	if (ar->scan.state != ATH11K_SCAN_IDLE)
		__ath11k_scan_finish(ar);
	spin_unlock_bh(&ar->data_lock);

	return ret;
}

static void ath11k_scan_abort(struct ath11k *ar)
{
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	spin_lock_bh(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH11K_SCAN_IDLE:
		/* This can happen if timeout worker kicked in and called
		 * abortion while scan completion was being processed.
		 */
		break;
	case ATH11K_SCAN_STARTING:
	case ATH11K_SCAN_ABORTING:
		ath11k_warn(ar->ab, "refusing scan abortion due to invalid scan state: %d\n",
			    ar->scan.state);
		break;
	case ATH11K_SCAN_RUNNING:
		ar->scan.state = ATH11K_SCAN_ABORTING;
		spin_unlock_bh(&ar->data_lock);

		ret = ath11k_scan_stop(ar);
		if (ret)
			ath11k_warn(ar->ab, "failed to abort scan: %d\n", ret);

		spin_lock_bh(&ar->data_lock);
		break;
	}

	spin_unlock_bh(&ar->data_lock);
}

static void ath11k_scan_timeout_work(struct work_struct *work)
{
	struct ath11k *ar = container_of(work, struct ath11k,
					 scan.timeout.work);

	mutex_lock(&ar->conf_mutex);
	ath11k_scan_abort(ar);
	mutex_unlock(&ar->conf_mutex);
}

static int ath11k_start_scan(struct ath11k *ar,
			     struct scan_req_params *arg)
{
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	ret = ath11k_wmi_send_scan_start_cmd(ar, arg);
	if (ret)
		return ret;

	ret = wait_for_completion_timeout(&ar->scan.started, 1 * HZ);
	if (ret == 0) {
		ret = ath11k_scan_stop(ar);
		if (ret)
			ath11k_warn(ar->ab, "failed to stop scan: %d\n", ret);

		return -ETIMEDOUT;
	}

	/* If we failed to start the scan, return error code at
	 * this point.  This is probably due to some issue in the
	 * firmware, but no need to wedge the driver due to that...
	 */
	spin_lock_bh(&ar->data_lock);
	if (ar->scan.state == ATH11K_SCAN_IDLE) {
		spin_unlock_bh(&ar->data_lock);
		return -EINVAL;
	}
	spin_unlock_bh(&ar->data_lock);

	return 0;
}

static int ath11k_hw_scan(struct ieee80211_hw *hw,
			  struct ieee80211_vif *vif,
			  struct ieee80211_scan_request *hw_req)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	struct cfg80211_scan_request *req = &hw_req->req;
	struct scan_req_params arg;
	int ret = 0;
	int i;

	mutex_lock(&ar->conf_mutex);

	spin_lock_bh(&ar->data_lock);
	switch (ar->scan.state) {
	case ATH11K_SCAN_IDLE:
		reinit_completion(&ar->scan.started);
		reinit_completion(&ar->scan.completed);
		ar->scan.state = ATH11K_SCAN_STARTING;
		ar->scan.is_roc = false;
		ar->scan.vdev_id = arvif->vdev_id;
		ret = 0;
		break;
	case ATH11K_SCAN_STARTING:
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		ret = -EBUSY;
		break;
	}
	spin_unlock_bh(&ar->data_lock);

	if (ret)
		goto exit;

	memset(&arg, 0, sizeof(arg));
	ath11k_wmi_start_scan_init(ar, &arg);
	arg.vdev_id = arvif->vdev_id;
	arg.scan_id = ATH11K_SCAN_ID;

	if (req->ie_len) {
		arg.extraie.len = req->ie_len;
		arg.extraie.ptr = kzalloc(req->ie_len, GFP_KERNEL);
		memcpy(arg.extraie.ptr, req->ie, req->ie_len);
	}

	if (req->n_ssids) {
		arg.num_ssids = req->n_ssids;
		for (i = 0; i < arg.num_ssids; i++) {
			arg.ssid[i].length  = req->ssids[i].ssid_len;
			memcpy(&arg.ssid[i].ssid, req->ssids[i].ssid,
			       req->ssids[i].ssid_len);
		}
	} else {
		arg.scan_flags |= WMI_SCAN_FLAG_PASSIVE;
	}

	if (req->n_channels) {
		arg.num_chan = req->n_channels;
		for (i = 0; i < arg.num_chan; i++)
			arg.chan_list[i] = req->channels[i]->center_freq;
	}

	ret = ath11k_start_scan(ar, &arg);
	if (ret) {
		ath11k_warn(ar->ab, "failed to start hw scan: %d\n", ret);
		spin_lock_bh(&ar->data_lock);
		ar->scan.state = ATH11K_SCAN_IDLE;
		spin_unlock_bh(&ar->data_lock);
	}

	/* Add a 200ms margin to account for event/command processing */
	ieee80211_queue_delayed_work(ar->hw, &ar->scan.timeout,
				     msecs_to_jiffies(arg.max_scan_time +
						      200));

exit:
	if (req->ie_len)
		kfree(arg.extraie.ptr);

	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static void ath11k_cancel_hw_scan(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif)
{
	struct ath11k *ar = hw->priv;

	mutex_lock(&ar->conf_mutex);
	ath11k_scan_abort(ar);
	mutex_unlock(&ar->conf_mutex);

	cancel_delayed_work_sync(&ar->scan.timeout);
}

static int ath11k_install_key(struct ath11k_vif *arvif,
			      struct ieee80211_key_conf *key,
			      enum set_key_cmd cmd,
			      const u8 *macaddr, u32 flags)
{
	int ret;
	struct ath11k *ar = arvif->ar;
	struct wmi_vdev_install_key_arg arg = {
		.vdev_id = arvif->vdev_id,
		.key_idx = key->keyidx,
		.key_len = key->keylen,
		.key_data = key->key,
		.key_flags = flags,
		.macaddr = macaddr,
	};

	lockdep_assert_held(&arvif->ar->conf_mutex);

	reinit_completion(&ar->install_key_done);

	if (cmd == DISABLE_KEY) {
		/* TODO: Check if FW expects  value other than NONE for del */
		/* arg.key_cipher = WMI_CIPHER_NONE; */
		arg.key_len = 0;
		arg.key_data = NULL;
		goto install;
	}

	switch (key->cipher) {
	case WLAN_CIPHER_SUITE_CCMP:
		arg.key_cipher = WMI_CIPHER_AES_CCM;
		/* TODO: Re-check if flag is valid */
		key->flags |= IEEE80211_KEY_FLAG_GENERATE_IV_MGMT;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		arg.key_cipher = WMI_CIPHER_TKIP;
		arg.key_txmic_len = 8;
		arg.key_rxmic_len = 8;
		break;
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		arg.key_cipher = WMI_CIPHER_WEP;
		break;
	case WLAN_CIPHER_SUITE_CCMP_256:
		arg.key_cipher = WMI_CIPHER_AES_CCM;
		break;
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
		arg.key_cipher = WMI_CIPHER_AES_GCM;
		break;
	case WLAN_CIPHER_SUITE_BIP_GMAC_128:
	case WLAN_CIPHER_SUITE_BIP_GMAC_256:
		arg.key_cipher = WMI_CIPHER_AES_GMAC;
	case WLAN_CIPHER_SUITE_AES_CMAC:
	case WLAN_CIPHER_SUITE_BIP_CMAC_256:
		arg.key_cipher = WMI_CIPHER_AES_CMAC;
	default:
		ath11k_warn(ar->ab, "cipher %d is not supported\n", key->cipher);
		return -EOPNOTSUPP;
	}

install:
	ret = ath11k_wmi_vdev_install_key(arvif->ar, &arg);
	if (ret)
		return ret;

	if (!wait_for_completion_timeout(&ar->install_key_done, 1 * HZ))
		return -ETIMEDOUT;

	return ar->install_key_status ? -EINVAL : 0;
}

bool ath11k_mac_is_peer_wep_key_set(struct ath11k_base *ab, const u8 *addr,
				    u8 keyidx)
{
	struct ath11k_peer *peer;
	int i;

	lockdep_assert_held(&ab->data_lock);

	peer = ath11k_peer_find_by_addr(ab, addr);
	if (!peer)
		return false;

	for (i = 0; i < ARRAY_SIZE(peer->keys); i++) {
		if (peer->keys[i] && peer->keys[i]->keyidx == keyidx)
			return true;
	}

	return false;
}

static int ath11k_clear_vdev_key(struct ath11k_vif *arvif,
				 struct ieee80211_key_conf *key)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_peer *peer;
	u8 addr[ETH_ALEN];
	int first_errno = 0;
	int ret;
	int i;
	u32 flags = 0;

	lockdep_assert_held(&ar->conf_mutex);

	for (;;) {
		/* In ath11k_install_key, we can't hold data_lock longer,
		 * so we try to remove the keys incrementally
		 */
		spin_lock_bh(&ar->data_lock);
		i = 0;
		list_for_each_entry(peer, &ar->ab->peers, list) {
			for (i = 0; i < ARRAY_SIZE(peer->keys); i++) {
				if (peer->keys[i] == key) {
					ether_addr_copy(addr, peer->addr);
					peer->keys[i] = NULL;
					break;
				}
			}

			if (i < ARRAY_SIZE(peer->keys))
				break;
		}
		spin_unlock_bh(&ar->data_lock);

		if (i == ARRAY_SIZE(peer->keys))
			break;
		/* key flags are not required to delete the key */
		ret = ath11k_install_key(arvif, key, DISABLE_KEY, addr, flags);
		if (ret < 0 && first_errno == 0)
			first_errno = ret;

		if (ret)
			ath11k_warn(ar->ab, "failed to remove key for %pM: %d\n",
				    addr, ret);
	}

	return first_errno;
}

static int ath11k_install_peer_wep_keys(struct ath11k_vif *arvif,
					const u8 *addr)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_peer *peer;
	int ret;
	int i;
	u32 flags;

	lockdep_assert_held(&ar->conf_mutex);

	if (WARN_ON(arvif->vif->type != NL80211_IFTYPE_AP))
		return -EINVAL;

	spin_lock_bh(&ar->data_lock);
	peer = ath11k_peer_find(ar->ab, arvif->vdev_id, addr);
	spin_unlock_bh(&ar->data_lock);

	if (!peer)
		return -ENOENT;

	for (i = 0; i < ARRAY_SIZE(arvif->wep_keys); i++) {
		if (arvif->wep_keys[i] == NULL)
			continue;

		flags = WMI_KEY_PAIRWISE;

		if (arvif->def_wep_key_index == i)
			flags |= WMI_KEY_TX_USAGE;

		ret = ath11k_install_key(arvif, arvif->wep_keys[i],
					 SET_KEY, addr, flags);
		if (ret < 0)
			return ret;

		spin_lock_bh(&ar->data_lock);
		peer->keys[i] = arvif->wep_keys[i];
		spin_unlock_bh(&ar->data_lock);
	}

	return 0;
}

static int ath11k_clear_peer_keys(struct ath11k_vif *arvif,
				  const u8 *addr)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_peer *peer;
	int first_errno = 0;
	int ret;
	int i;
	u32 flags = 0;

	lockdep_assert_held(&ar->conf_mutex);

	spin_lock_bh(&ar->data_lock);
	peer = ath11k_peer_find(ar->ab, arvif->vdev_id, addr);
	spin_unlock_bh(&ar->data_lock);

	if (!peer)
		return -ENOENT;

	for (i = 0; i < ARRAY_SIZE(peer->keys); i++) {
		if (!peer->keys[i])
			continue;

		/* key flags are not required to delete the key */
		ret = ath11k_install_key(arvif, peer->keys[i],
					 DISABLE_KEY, addr, flags);
		if (ret < 0 && first_errno == 0)
			first_errno = ret;

		if (ret < 0)
			ath11k_warn(ar->ab, "failed to remove peer wep key %d: %d\n",
				    i, ret);

		spin_lock_bh(&ar->data_lock);
		peer->keys[i] = NULL;
		spin_unlock_bh(&ar->data_lock);
	}

	return first_errno;
}

static int ath11k_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			  struct ieee80211_vif *vif, struct ieee80211_sta *sta,
			  struct ieee80211_key_conf *key)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	struct ath11k_peer *peer;
	struct ath11k_sta *arsta;
	const u8 *peer_addr;
	bool is_wep = key->cipher == WLAN_CIPHER_SUITE_WEP40 ||
		key->cipher == WLAN_CIPHER_SUITE_WEP104;
	int ret = 0;
	u32 flags = 0;

	if (key->keyidx > WMI_MAX_KEY_INDEX)
		return -ENOSPC;

	mutex_lock(&ar->conf_mutex);

	if (sta)
		peer_addr = sta->addr;
	else if (arvif->vdev_type == WMI_VDEV_TYPE_STA)
		peer_addr = vif->bss_conf.bssid;
	else
		peer_addr = vif->addr;

	key->hw_key_idx = key->keyidx;

	/* the peer should not disappear in mid-way (unless FW goes awry) since
	 * we already hold conf_mutex. we just make sure its there now.
	 */
	spin_lock_bh(&ar->data_lock);
	peer = ath11k_peer_find(ar->ab, arvif->vdev_id, peer_addr);
	spin_unlock_bh(&ar->data_lock);

	if (!peer) {
		if (cmd == SET_KEY) {
			ath11k_warn(ar->ab, "cannot install key for non-existent peer %pM\n",
				    peer_addr);
			ret = -EOPNOTSUPP;
			goto exit;
		} else {
			/* if the peer doesn't exist there is no key to disable
			 * anymore
			 */
			goto exit;
		}
	}

	if (key->flags & IEEE80211_KEY_FLAG_PAIRWISE)
		flags |= WMI_KEY_PAIRWISE;
	else
		flags |= WMI_KEY_GROUP;

	if (is_wep) {
		if (cmd == SET_KEY) {
			arvif->wep_keys[key->keyidx] = key;
			if (arvif->def_wep_key_index == -1)
				flags |= WMI_KEY_TX_USAGE;
		} else {
			arvif->wep_keys[key->keyidx] = NULL;
			ath11k_clear_vdev_key(arvif, key);
		}
		/* TODO: mcast fix(ath10k) for fw applicable? */
	}

	ret = ath11k_install_key(arvif, key, cmd, peer_addr, flags);
	if (ret) {
		ath11k_warn(ar->ab, "ath11k_install_key failed (%d)\n", ret);
		goto exit;
	}

	ret = ath11k_dp_peer_rx_pn_replay_config(arvif, peer_addr, cmd, key);
	if (ret) {
		/* No need to fail because we can still perform PN replay
		 * detection check in mac80211.
		 */
		ath11k_dbg(ar->ab, ATH11K_DBG_MAC,
			   "failed to enable PN replay detection offload support %d\n",
			   ret);
	}

	spin_lock_bh(&ar->data_lock);
	peer = ath11k_peer_find(ar->ab, arvif->vdev_id, peer_addr);
	if (peer && cmd == SET_KEY)
		peer->keys[key->keyidx] = key;
	else if (peer && cmd == DISABLE_KEY)
		peer->keys[key->keyidx] = NULL;
	else if (!peer)
		/* impossible unless FW goes crazy */
		ath11k_warn(ar->ab, "peer %pM disappeared!\n", peer_addr);

	if (sta) {
		arsta = (struct ath11k_sta *)sta->drv_priv;

		switch (key->cipher) {
		case WLAN_CIPHER_SUITE_TKIP:
		case WLAN_CIPHER_SUITE_CCMP:
		case WLAN_CIPHER_SUITE_CCMP_256:
		case WLAN_CIPHER_SUITE_GCMP:
		case WLAN_CIPHER_SUITE_GCMP_256:
			if (cmd == SET_KEY)
				arsta->pn_type = HAL_PN_TYPE_WPA;
			else
				arsta->pn_type = HAL_PN_TYPE_NONE;
			break;
		default:
			arsta->pn_type = HAL_PN_TYPE_NONE;
			break;
		}
	}
	spin_unlock_bh(&ar->data_lock);

exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static void ath11k_set_default_unicast_key(struct ieee80211_hw *hw,
					   struct ieee80211_vif *vif,
					   int keyidx)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	int ret;

	mutex_lock(&arvif->ar->conf_mutex);

	if (arvif->ar->state != ATH11K_STATE_ON)
		goto unlock;

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac vdev %d set keyidx %d\n",
		   arvif->vdev_id, keyidx);

	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_DEF_KEYID, keyidx);

	if (ret) {
		ath11k_warn(ar->ab, "failed to update wep key index for vdev %d: %d\n",
			    arvif->vdev_id,
			    ret);
		goto unlock;
	}

	arvif->def_wep_key_index = keyidx;

unlock:
	mutex_unlock(&arvif->ar->conf_mutex);
}

static int
ath11k_mac_bitrate_mask_num_vht_rates(struct ath11k *ar,
				      enum nl80211_band band,
				      const struct cfg80211_bitrate_mask *mask)
{
	int num_rates = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].vht_mcs); i++)
		num_rates += hweight16(mask->control[band].vht_mcs[i]);

	return num_rates;
}

static int
ath11k_mac_set_peer_vht_fixed_rate(struct ath11k_vif *arvif,
				   struct ieee80211_sta *sta,
				   const struct cfg80211_bitrate_mask *mask,
				   enum nl80211_band band)
{
	struct ath11k *ar;
	u8 vht_rate, nss;
	u32 rate_code;
	int ret, i;

	lockdep_assert_held(&ar->conf_mutex);

	nss = 0;
	ar = arvif->ar;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].vht_mcs); i++) {
		if (hweight16(mask->control[band].vht_mcs[i]) == 1) {
			nss = i + 1;
			vht_rate = ffs(mask->control[band].vht_mcs[i]) - 1;
		}
	}

	if (!nss) {
		ath11k_warn(ar->ab, "No single VHT Fixed rate found to set for %pM",
			    sta->addr);
		return -EINVAL;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC,
		   "Setting Fixed VHT Rate for peer %pM. Device will not switch to any other selected rates",
		   sta->addr);

	rate_code = ATH11K_HW_RATE_CODE(vht_rate, nss,
					WMI_RATE_PREAMBLE_VHT);
	ret = ath11k_wmi_set_peer_param(ar, sta->addr,
					arvif->vdev_id,
					WMI_PEER_PARAM_FIXED_RATE,
					rate_code);
	if (ret)
		ath11k_warn(ar->ab,
			    "failed to update STA %pM Fixed Rate %d: %d\n",
			     sta->addr, rate_code, ret);

	return ret;
}

static int ath11k_station_assoc(struct ath11k *ar,
				struct ieee80211_vif *vif,
				struct ieee80211_sta *sta,
				bool reassoc)
{
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	struct peer_assoc_params peer_arg;
	int ret = 0;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	struct cfg80211_bitrate_mask *mask;
	u8 num_vht_rates;

	lockdep_assert_held(&ar->conf_mutex);

	if (WARN_ON(ath11k_mac_vif_chan(vif, &def)))
		return -EPERM;

	band = def.chan->band;
	mask = &arvif->bitrate_mask;

	ath11k_peer_assoc_prepare(ar, vif, sta, &peer_arg, reassoc);

	ret = ath11k_wmi_send_peer_assoc_cmd(ar, &peer_arg);
	if (ret) {
		ath11k_warn(ar->ab, "failed to run peer assoc for STA %pM vdev %i: %d\n",
			    sta->addr, arvif->vdev_id, ret);
		return ret;
	}

	if (!wait_for_completion_timeout(&ar->peer_assoc_done, 1 * HZ)) {
		ath11k_warn(ar->ab, "failed to get peer assoc conf event for %pM vdev %i\n",
			    sta->addr, arvif->vdev_id);
		return -ETIMEDOUT;
	}

	num_vht_rates = ath11k_mac_bitrate_mask_num_vht_rates(ar, band, mask);

	/* If single VHT rate is configured (by set_bitrate_mask()),
	 * peer_assoc will disable VHT. This is now enabled by a peer specific
	 * fixed param.
	 * Note that all other rates and NSS will be disabled for this peer.
	 */
	if (sta->vht_cap.vht_supported && num_vht_rates == 1)
		ret = ath11k_mac_set_peer_vht_fixed_rate(arvif, sta, mask,
							 band);
		if (ret)
			return ret;

	if (!sta->wme) {
		arvif->num_legacy_stations++;
		ret = ath11k_recalc_rtscts_prot(arvif);
		if (ret)
			return ret;
	}

	/* Plumb cached keys only for static WEP */
	if ((arvif->def_wep_key_index != -1) && !reassoc) {
		ret = ath11k_install_peer_wep_keys(arvif, sta->addr);
		if (ret) {
			ath11k_warn(ar->ab,
				    "failed to install peer wep keys for vdev %i: %d\n",
				    arvif->vdev_id, ret);
			return ret;
		}
	}

	/* TODO: per-STA AP PS(UAPSD,MAX_SP) settings */

	return 0;
}

static int ath11k_station_disassoc(struct ath11k *ar,
				   struct ieee80211_vif *vif,
				   struct ieee80211_sta *sta)
{
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	int ret = 0;

	lockdep_assert_held(&ar->conf_mutex);

	if (!sta->wme) {
		arvif->num_legacy_stations--;
		ret = ath11k_recalc_rtscts_prot(arvif);
		if (ret)
			return ret;
	}

	ret = ath11k_clear_peer_keys(arvif, sta->addr);
	if (ret) {
		ath11k_warn(ar->ab, "failed to clear all peer wep keys for vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}
	return 0;
}

static void ath11k_sta_rc_update_wk(struct work_struct *wk)
{
	struct ath11k *ar;
	struct ath11k_vif *arvif;
	struct ath11k_sta *arsta;
	struct ieee80211_sta *sta;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	const u8 *ht_mcs_mask;
	const u16 *vht_mcs_mask;
	u32 changed, bw, nss, smps;
	int err, num_vht_rates;
	const struct cfg80211_bitrate_mask *mask;
	struct peer_assoc_params peer_arg;

	arsta = container_of(wk, struct ath11k_sta, update_wk);
	sta = container_of((void *)arsta, struct ieee80211_sta, drv_priv);
	arvif = arsta->arvif;
	ar = arvif->ar;

	if (WARN_ON(ath11k_mac_vif_chan(arvif->vif, &def)))
		return;

	band = def.chan->band;
	ht_mcs_mask = arvif->bitrate_mask.control[band].ht_mcs;
	vht_mcs_mask = arvif->bitrate_mask.control[band].vht_mcs;

	spin_lock_bh(&ar->data_lock);

	changed = arsta->changed;
	arsta->changed = 0;

	bw = arsta->bw;
	nss = arsta->nss;
	smps = arsta->smps;

	spin_unlock_bh(&ar->data_lock);

	mutex_lock(&ar->conf_mutex);

	nss = max_t(u32, 1, nss);
	nss = min(nss, max(ath11k_mac_max_ht_nss(ht_mcs_mask),
			   ath11k_mac_max_vht_nss(vht_mcs_mask)));

	if (changed & IEEE80211_RC_BW_CHANGED) {
		err = ath11k_wmi_set_peer_param(ar, sta->addr, arvif->vdev_id,
						WMI_PEER_CHWIDTH, bw);
		if (err)
			ath11k_warn(ar->ab, "failed to update STA %pM peer bw %d: %d\n",
				    sta->addr, bw, err);
	}

	if (changed & IEEE80211_RC_NSS_CHANGED) {
		ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac update sta %pM nss %d\n",
			   sta->addr, nss);

		err = ath11k_wmi_set_peer_param(ar, sta->addr, arvif->vdev_id,
						WMI_PEER_NSS, nss);
		if (err)
			ath11k_warn(ar->ab, "failed to update STA %pM nss %d: %d\n",
				    sta->addr, nss, err);
	}

	if (changed & IEEE80211_RC_SMPS_CHANGED) {
		ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac update sta %pM smps %d\n",
			   sta->addr, smps);

		err = ath11k_wmi_set_peer_param(ar, sta->addr, arvif->vdev_id,
						WMI_PEER_MIMO_PS_STATE, smps);
		if (err)
			ath11k_warn(ar->ab, "failed to update STA %pM smps %d: %d\n",
				    sta->addr, smps, err);
	}

	if (changed & IEEE80211_RC_SUPP_RATES_CHANGED) {
		mask = &arvif->bitrate_mask;
		num_vht_rates = ath11k_mac_bitrate_mask_num_vht_rates(ar, band,
								      mask);

		/* Peer_assoc_prepare will reject vht rates in
		 * bitrate_mask if its not available in range format and
		 * sets vht tx_rateset as unsupported. So multiple VHT MCS
		 * setting(eg. MCS 4,5,6) per peer is not supported here.
		 * But, Single rate in VHT mask can be set as per-peer
		 * fixed rate. But even if any HT rates are configured in
		 * the bitrate mask, device will not switch to those rates
		 * when per-peer Fixed rate is set.
		 * TODO: Check RATEMASK_CMDID to support auto rates selection
		 * across HT/VHT and for multiple VHT MCS support.
		 */
		if (sta->vht_cap.vht_supported && num_vht_rates == 1) {
			ath11k_mac_set_peer_vht_fixed_rate(arvif, sta, mask,
							   band);
		} else {
			/* If the peer is non-VHT or no fixed VHT rate
			 * is provided in the new bitrate mask we set the
			 * other rates using peer_assoc command.
			 */
			ath11k_peer_assoc_prepare(ar, arvif->vif, sta,
						  &peer_arg, true);

			err = ath11k_wmi_send_peer_assoc_cmd(ar, &peer_arg);
			if (err)
				ath11k_warn(ar->ab, "failed to run peer assoc for STA %pM vdev %i: %d\n",
					    sta->addr, arvif->vdev_id, err);

			if (!wait_for_completion_timeout(&ar->peer_assoc_done, 1 * HZ))
				ath11k_warn(ar->ab, "failed to get peer assoc conf event for %pM vdev %i\n",
					    sta->addr, arvif->vdev_id);

		}
	}

	mutex_unlock(&ar->conf_mutex);
}

static int ath11k_sta_state(struct ieee80211_hw *hw,
			    struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta,
			    enum ieee80211_sta_state old_state,
			    enum ieee80211_sta_state new_state)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	struct ath11k_sta *arsta = (struct ath11k_sta *)sta->drv_priv;
	struct peer_create_params peer_param;
	int ret = 0;

	/* cancel must be done outside the mutex to avoid deadlock */
	if ((old_state == IEEE80211_STA_NONE &&
	     new_state == IEEE80211_STA_NOTEXIST))
		cancel_work_sync(&arsta->update_wk);

	mutex_lock(&ar->conf_mutex);

	if (old_state == IEEE80211_STA_NOTEXIST &&
	    new_state == IEEE80211_STA_NONE) {
		memset(arsta, 0, sizeof(*arsta));
		arsta->arvif = arvif;
		INIT_WORK(&arsta->update_wk, ath11k_sta_rc_update_wk);

		peer_param.vdev_id = arvif->vdev_id;
		peer_param.peer_addr = sta->addr;
		peer_param.peer_type = WMI_PEER_TYPE_DEFAULT;
		ret = ath11k_peer_create(ar, arvif, sta, &peer_param);
		if (ret) {
			ath11k_warn(ar->ab, "Failed to add peer: %pM for VDEV: %d\n",
				    sta->addr, arvif->vdev_id);
			goto exit;
		}

		ath11k_info(ar->ab, "Added peer: %pM for VDEV: %d\n",
			    sta->addr, arvif->vdev_id);

		if (ath11k_debug_is_extd_tx_stats_enabled(ar)) {
			arsta->tx_stats = kzalloc(sizeof(*arsta->tx_stats),
						  GFP_KERNEL);
			if (!arsta->tx_stats)
				goto exit;
		}

		ret = ath11k_dp_peer_setup(ar, arvif->vdev_id, sta->addr);
		if (ret) {
			ath11k_warn(ar->ab, "failed to setup dp for peer %pM on vdev %i (%d)\n",
				    sta->addr, arvif->vdev_id, ret);
			ath11k_peer_delete(ar, arvif->vdev_id, sta->addr);
		}
	} else if ((old_state == IEEE80211_STA_NONE &&
		    new_state == IEEE80211_STA_NOTEXIST)) {
		ath11k_dp_peer_cleanup(ar, arvif->vdev_id, sta->addr);

		ret = ath11k_peer_delete(ar, arvif->vdev_id, sta->addr);
		if (ret)
			ath11k_warn(ar->ab, "Failed to delete peer: %pM for VDEV: %d\n",
				    sta->addr, arvif->vdev_id);
		else
			ath11k_info(ar->ab,
				    "Removed peer: %pM for VDEV: %d\n",
				    sta->addr, arvif->vdev_id);

		if (ath11k_debug_is_extd_tx_stats_enabled(ar))
			kfree(arsta->tx_stats);
	} else if (old_state == IEEE80211_STA_AUTH &&
		   new_state == IEEE80211_STA_ASSOC &&
		   (vif->type == NL80211_IFTYPE_AP ||
		    vif->type == NL80211_IFTYPE_ADHOC)) {
		ret = ath11k_station_assoc(ar, vif, sta, false);
		if (ret)
			ath11k_warn(ar->ab, "Failed to associate station: %pM\n",
				    sta->addr);
		else
			ath11k_info(ar->ab,
				    "Station %pM moved to assoc state\n",
				    sta->addr);
	} else if (old_state == IEEE80211_STA_ASSOC &&
		   new_state == IEEE80211_STA_AUTH &&
		   (vif->type == NL80211_IFTYPE_AP ||
		    vif->type == NL80211_IFTYPE_ADHOC)) {
		ret = ath11k_station_disassoc(ar, vif, sta);
		if (ret)
			ath11k_warn(ar->ab, "Failed to disassociate station: %pM\n",
				    sta->addr);
		else
			ath11k_info(ar->ab,
				    "Station %pM moved to disassociated state\n",
				    sta->addr);
	}

exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static void ath11k_sta_rc_update(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 struct ieee80211_sta *sta,
				 u32 changed)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_sta *arsta = (struct ath11k_sta *)sta->drv_priv;
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	struct ath11k_peer *peer;
	u32 bw, smps;

	spin_lock_bh(&ar->data_lock);

	peer = ath11k_peer_find(ar->ab, arvif->vdev_id, sta->addr);
	if (!peer) {
		spin_unlock_bh(&ar->data_lock);
		ath11k_warn(ar->ab, "mac sta rc update failed to find peer %pM on vdev %i\n",
			    sta->addr, arvif->vdev_id);
		return;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC,
		   "mac sta rc update for %pM changed %08x bw %d nss %d smps %d\n",
		   sta->addr, changed, sta->bandwidth, sta->rx_nss,
		   sta->smps_mode);

	if (changed & IEEE80211_RC_BW_CHANGED) {
		bw = WMI_PEER_CHWIDTH_20MHZ;

		switch (sta->bandwidth) {
		case IEEE80211_STA_RX_BW_20:
			bw = WMI_PEER_CHWIDTH_20MHZ;
			break;
		case IEEE80211_STA_RX_BW_40:
			bw = WMI_PEER_CHWIDTH_40MHZ;
			break;
		case IEEE80211_STA_RX_BW_80:
			bw = WMI_PEER_CHWIDTH_80MHZ;
			break;
		case IEEE80211_STA_RX_BW_160:
			bw = WMI_PEER_CHWIDTH_160MHZ;
			break;
		default:
			ath11k_warn(ar->ab, "Invalid bandwidth %d in rc update for %pM\n",
				    sta->bandwidth, sta->addr);
			bw = WMI_PEER_CHWIDTH_20MHZ;
			break;
		}

		arsta->bw = bw;
	}

	if (changed & IEEE80211_RC_NSS_CHANGED)
		arsta->nss = sta->rx_nss;

	if (changed & IEEE80211_RC_SMPS_CHANGED) {
		smps = WMI_PEER_SMPS_PS_NONE;

		switch (sta->smps_mode) {
		case IEEE80211_SMPS_AUTOMATIC:
		case IEEE80211_SMPS_OFF:
			smps = WMI_PEER_SMPS_PS_NONE;
			break;
		case IEEE80211_SMPS_STATIC:
			smps = WMI_PEER_SMPS_STATIC;
			break;
		case IEEE80211_SMPS_DYNAMIC:
			smps = WMI_PEER_SMPS_DYNAMIC;
			break;
		default:
			ath11k_warn(ar->ab, "Invalid smps %d in sta rc update for %pM\n",
				    sta->smps_mode, sta->addr);
			smps = WMI_PEER_SMPS_PS_NONE;
			break;
		}

		arsta->smps = smps;
	}

	arsta->changed |= changed;

	spin_unlock_bh(&ar->data_lock);

	ieee80211_queue_work(hw, &arsta->update_wk);
}

static int ath11k_conf_tx_uapsd(struct ath11k *ar, struct ieee80211_vif *vif,
				u16 ac, bool enable)
{
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	u32 value = 0;
	int ret = 0;

	if (arvif->vdev_type != WMI_VDEV_TYPE_STA)
		return 0;

	switch (ac) {
	case IEEE80211_AC_VO:
		value = WMI_STA_PS_UAPSD_AC3_DELIVERY_EN |
			WMI_STA_PS_UAPSD_AC3_TRIGGER_EN;
		break;
	case IEEE80211_AC_VI:
		value = WMI_STA_PS_UAPSD_AC2_DELIVERY_EN |
			WMI_STA_PS_UAPSD_AC2_TRIGGER_EN;
		break;
	case IEEE80211_AC_BE:
		value = WMI_STA_PS_UAPSD_AC1_DELIVERY_EN |
			WMI_STA_PS_UAPSD_AC1_TRIGGER_EN;
		break;
	case IEEE80211_AC_BK:
		value = WMI_STA_PS_UAPSD_AC0_DELIVERY_EN |
			WMI_STA_PS_UAPSD_AC0_TRIGGER_EN;
		break;
	}

	if (enable)
		arvif->u.sta.uapsd |= value;
	else
		arvif->u.sta.uapsd &= ~value;

	ret = ath11k_wmi_set_sta_ps_param(ar, arvif->vdev_id,
					  WMI_STA_PS_PARAM_UAPSD,
					  arvif->u.sta.uapsd);
	if (ret) {
		ath11k_warn(ar->ab, "could not set uapsd params %d\n", ret);
		goto exit;
	}

	if (arvif->u.sta.uapsd)
		value = WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD;
	else
		value = WMI_STA_PS_RX_WAKE_POLICY_WAKE;

	ret = ath11k_wmi_set_sta_ps_param(ar, arvif->vdev_id,
					  WMI_STA_PS_PARAM_RX_WAKE_POLICY,
					  value);
	if (ret)
		ath11k_warn(ar->ab, "could not set rx wake param %d\n", ret);

exit:
	return ret;
}

static int ath11k_conf_tx(struct ieee80211_hw *hw,
			  struct ieee80211_vif *vif, u16 ac,
			  const struct ieee80211_tx_queue_params *params)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	struct wmi_wmm_params_arg *p = NULL;
	int ret;

	mutex_lock(&ar->conf_mutex);

	switch (ac) {
	case IEEE80211_AC_VO:
		p = &arvif->wmm_params.ac_vo;
		break;
	case IEEE80211_AC_VI:
		p = &arvif->wmm_params.ac_vi;
		break;
	case IEEE80211_AC_BE:
		p = &arvif->wmm_params.ac_be;
		break;
	case IEEE80211_AC_BK:
		p = &arvif->wmm_params.ac_bk;
		break;
	}

	if (WARN_ON(!p)) {
		ret = -EINVAL;
		goto exit;
	}

	p->cwmin = params->cw_min;
	p->cwmax = params->cw_max;
	p->aifs = params->aifs;

	/* The channel time duration programmed in the HW is in absolute
	 * microseconds, while mac80211 gives the txop in units of
	 * 32 microseconds.
	 */
	p->txop = params->txop * 32;

	ret = ath11k_wmi_send_wmm_update_cmd_tlv(ar, arvif->vdev_id,
						 &arvif->wmm_params);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set wmm params: %d\n", ret);
		goto exit;
	}

	ret = ath11k_conf_tx_uapsd(ar, vif, ac, params->uapsd);

	if (ret)
		ath11k_warn(ar->ab, "failed to set sta uapsd: %d\n", ret);

exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static struct ieee80211_sta_ht_cap
ath11k_create_ht_cap(struct ath11k *ar, u32 ar_ht_cap)
{
	int i;
	struct ieee80211_sta_ht_cap ht_cap = {0};
	u32 ar_vht_cap = ar->pdev->cap.vht_cap;

	if (!(ar_ht_cap & WMI_HT_CAP_ENABLED))
		return ht_cap;

	ht_cap.ht_supported = 1;
	ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;
	ht_cap.ampdu_density = IEEE80211_HT_MPDU_DENSITY_8;
	ht_cap.cap |= IEEE80211_HT_CAP_SUP_WIDTH_20_40;
	ht_cap.cap |= IEEE80211_HT_CAP_DSSSCCK40;
	ht_cap.cap |= WLAN_HT_CAP_SM_PS_STATIC << IEEE80211_HT_CAP_SM_PS_SHIFT;

	if (ar_ht_cap & WMI_HT_CAP_HT20_SGI)
		ht_cap.cap |= IEEE80211_HT_CAP_SGI_20;

	if (ar_ht_cap & WMI_HT_CAP_HT40_SGI)
		ht_cap.cap |= IEEE80211_HT_CAP_SGI_40;

	if (ar_ht_cap & WMI_HT_CAP_DYNAMIC_SMPS) {
		u32 smps;

		smps   = WLAN_HT_CAP_SM_PS_DYNAMIC;
		smps <<= IEEE80211_HT_CAP_SM_PS_SHIFT;

		ht_cap.cap |= smps;
	}

	if (ar_ht_cap & WMI_HT_CAP_TX_STBC)
		ht_cap.cap |= IEEE80211_HT_CAP_TX_STBC;

	if (ar_ht_cap & WMI_HT_CAP_RX_STBC) {
		u32 stbc;

		stbc   = ar_ht_cap;
		stbc  &= WMI_HT_CAP_RX_STBC;
		stbc >>= WMI_HT_CAP_RX_STBC_MASK_SHIFT;
		stbc <<= IEEE80211_HT_CAP_RX_STBC_SHIFT;
		stbc  &= IEEE80211_HT_CAP_RX_STBC;

		ht_cap.cap |= stbc;
	}

	if (ar_ht_cap & WMI_HT_CAP_RX_LDPC)
		ht_cap.cap |= IEEE80211_HT_CAP_LDPC_CODING;

	if (ar_ht_cap & WMI_HT_CAP_L_SIG_TXOP_PROT)
		ht_cap.cap |= IEEE80211_HT_CAP_LSIG_TXOP_PROT;

	if (ar_vht_cap & WMI_VHT_CAP_MAX_MPDU_LEN_MASK)
		ht_cap.cap |= IEEE80211_HT_CAP_MAX_AMSDU;

	for (i = 0; i < ar->num_rx_chains; i++) {
		if (ar->cfg_rx_chainmask & BIT(i))
			ht_cap.mcs.rx_mask[i] = 0xFF;
	}

	ht_cap.mcs.tx_params |= IEEE80211_HT_MCS_TX_DEFINED;

	return ht_cap;
}

static int ath11k_mac_set_txbf_conf(struct ath11k_vif *arvif)
{
	u32 value = 0;
	struct ath11k *ar = arvif->ar;
	int nsts;
	int sound_dim;
	u32 vht_cap = ar->pdev->cap.vht_cap;
	u32 vdev_param = WMI_VDEV_PARAM_TXBF;

	if (vht_cap & (IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE)) {
		nsts = vht_cap & IEEE80211_VHT_CAP_BEAMFORMEE_STS_MASK;
		nsts >>= IEEE80211_VHT_CAP_BEAMFORMEE_STS_SHIFT;
		value |= SM(nsts, WMI_TXBF_STS_CAP_OFFSET);
	}

	if (vht_cap & (IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE)) {
		sound_dim = vht_cap &
			    IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_MASK;
		sound_dim >>= IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_SHIFT;
		if (sound_dim > (ar->num_tx_chains - 1))
			sound_dim = ar->num_tx_chains - 1;
		value |= SM(sound_dim, WMI_BF_SOUND_DIM_OFFSET);
	}

	if (!value)
		return 0;

	if (vht_cap & IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE) {
		value |= WMI_VDEV_PARAM_TXBF_SU_TX_BFER;

		if ((vht_cap & IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE) &&
		    arvif->vdev_type == WMI_VDEV_TYPE_AP)
			value |= WMI_VDEV_PARAM_TXBF_MU_TX_BFER;
	}

	/* TODO: SUBFEE not validated in HK, disable here until validated? */

	if (vht_cap & IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE) {
		value |= WMI_VDEV_PARAM_TXBF_SU_TX_BFEE;

		if ((vht_cap & IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE) &&
		    arvif->vdev_type == WMI_VDEV_TYPE_STA)
			value |= WMI_VDEV_PARAM_TXBF_MU_TX_BFEE;
	}

	return ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					     vdev_param, value);
}

static void ath11k_set_vht_txbf_cap(struct ath11k *ar, u32 *vht_cap)
{
	bool subfer, subfee;
	int sound_dim = 0;

	subfer = !!(*vht_cap & (IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE));
	subfee = !!(*vht_cap & (IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE));

	if (ar->num_tx_chains < 2) {
		*vht_cap &= ~(IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE);
		subfer = false;
	}

	/* If SU Beaformer is not set, then disable MU Beamformer Capability */
	if (!subfer)
		*vht_cap &= ~(IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE);

	/* If SU Beaformee is not set, then disable MU Beamformee Capability */
	if (!subfee)
		*vht_cap &= ~(IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE);

	sound_dim = (*vht_cap & IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_MASK);
	sound_dim >>= IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_SHIFT;
	*vht_cap &= ~IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_MASK;

	/* TODO: Need to check invalid STS and Sound_dim values set by FW? */

	/* Enable Sounding Dimension Field only if SU BF is enabled */
	if (subfer) {
		if (sound_dim > (ar->num_tx_chains - 1))
			sound_dim = ar->num_tx_chains - 1;

		sound_dim <<= IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_SHIFT;
		sound_dim &=  IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_MASK;
		*vht_cap |= sound_dim;
	}

	/* Use the STS advertised by FW unless SU Beamformee is not supported*/
	if (!subfee)
		*vht_cap &= ~(IEEE80211_VHT_CAP_BEAMFORMEE_STS_MASK);
}

static struct ieee80211_sta_vht_cap
ath11k_create_vht_cap(struct ath11k *ar)
{
	struct ieee80211_sta_vht_cap vht_cap = {0};
	u16 txmcs_map, rxmcs_map;
	int i;

	vht_cap.vht_supported = 1;
	vht_cap.cap = ar->pdev->cap.vht_cap;

	ath11k_set_vht_txbf_cap(ar, &vht_cap.cap);

	rxmcs_map = 0;
	txmcs_map = 0;
	for (i = 0; i < 8; i++) {
		if (i < ar->num_tx_chains && ar->cfg_tx_chainmask & BIT(i))
			txmcs_map |= IEEE80211_VHT_MCS_SUPPORT_0_9 << (i * 2);
		else
			txmcs_map |= IEEE80211_VHT_MCS_NOT_SUPPORTED << (i * 2);

		if (i < ar->num_rx_chains && ar->cfg_rx_chainmask & BIT(i))
			rxmcs_map |= IEEE80211_VHT_MCS_SUPPORT_0_9 << (i * 2);
		else
			rxmcs_map |= IEEE80211_VHT_MCS_NOT_SUPPORTED << (i * 2);
	}

	if (ar->cfg_tx_chainmask <= 1)
		vht_cap.cap &= ~IEEE80211_VHT_CAP_TXSTBC;

	vht_cap.vht_mcs.rx_mcs_map = cpu_to_le16(rxmcs_map);
	vht_cap.vht_mcs.tx_mcs_map = cpu_to_le16(txmcs_map);

	return vht_cap;
}

static void ath11k_mac_setup_ht_vht_cap(struct ath11k *ar,
					struct ath11k_pdev_cap *cap,
					u32 *ht_cap_info)
{
	struct ieee80211_supported_band *band;
	u32 ht_cap;

	if (cap->supported_bands & WMI_HOST_WLAN_2G_CAP) {
		band = &ar->mac.sbands[NL80211_BAND_2GHZ];
		ht_cap = cap->band[NL80211_BAND_2GHZ].ht_cap_info;
		if (ht_cap_info)
			*ht_cap_info = ht_cap;
		band->ht_cap = ath11k_create_ht_cap(ar, ht_cap);
	}

	if (cap->supported_bands & WMI_HOST_WLAN_5G_CAP) {
		band = &ar->mac.sbands[NL80211_BAND_5GHZ];
		ht_cap = cap->band[NL80211_BAND_5GHZ].ht_cap_info;
		if (ht_cap_info)
			*ht_cap_info = ht_cap;
		band->ht_cap = ath11k_create_ht_cap(ar, ht_cap);
		band->vht_cap = ath11k_create_vht_cap(ar);
	}
}

static int __ath11k_set_antenna(struct ath11k *ar, u32 tx_ant, u32 rx_ant)
{
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	ar->cfg_tx_chainmask = tx_ant;
	ar->cfg_rx_chainmask = rx_ant;

	if (ar->state != ATH11K_STATE_ON)
		return 0;

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_TX_CHAIN_MASK,
					tx_ant, ar->pdev->pdev_id);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set tx-chainmask: %d, req 0x%x\n",
			    ret, tx_ant);
		return ret;
	}

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_RX_CHAIN_MASK,
					rx_ant, ar->pdev->pdev_id);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set rx-chainmask: %d, req 0x%x\n",
			    ret, rx_ant);
		return ret;
	}

	/* Reload HT/VHT capability */
	ath11k_mac_setup_ht_vht_cap(ar, &ar->pdev->cap, NULL);

	return 0;
}

static int ath11k_mac_tx_mgmt_pending_free(int buf_id, void *skb, void *ctx)
{
	struct ath11k *ar = ctx;
	struct ath11k_base *ab = ar->ab;
	struct sk_buff *msdu = skb;
	struct ieee80211_tx_info *info;

	spin_lock_bh(&ar->txmgmt_idr_lock);
	idr_remove(&ar->txmgmt_idr, buf_id);
	spin_unlock_bh(&ar->txmgmt_idr_lock);
	dma_unmap_single(ab->dev, ATH11K_SKB_CB(msdu)->paddr, msdu->len,
			 DMA_TO_DEVICE);

	info = IEEE80211_SKB_CB(msdu);
	memset(&info->status, 0, sizeof(info->status));

	ieee80211_free_txskb(ar->hw, msdu);

	return 0;
}

static int ath11k_mac_vif_txmgmt_idr_remove(int buf_id, void *skb, void *ctx)
{
	struct ieee80211_vif *vif = ctx;
	struct ath11k_skb_cb *skb_cb = ATH11K_SKB_CB((struct sk_buff *)skb);
	struct sk_buff *msdu = skb;
	struct ath11k *ar = skb_cb->ar;
	struct ath11k_base *ab = ar->ab;

	if (skb_cb->vif == vif) {
		spin_lock_bh(&ar->txmgmt_idr_lock);
		idr_remove(&ar->txmgmt_idr, buf_id);
		spin_unlock_bh(&ar->txmgmt_idr_lock);
		dma_unmap_single(ab->dev, skb_cb->paddr, msdu->len,
				 DMA_TO_DEVICE);
	}

	return 0;
}

static int ath11k_mac_mgmt_tx_wmi(struct ath11k *ar, struct ath11k_vif *arvif,
				  struct sk_buff *skb)
{
	struct ath11k_base *ab = ar->ab;
	dma_addr_t paddr;
	int buf_id;
	int ret;

	spin_lock_bh(&ar->txmgmt_idr_lock);
	buf_id = idr_alloc(&ar->txmgmt_idr, skb, 0,
			   ATH11K_TX_MGMT_NUM_PENDING_MAX, GFP_ATOMIC);
	spin_unlock_bh(&ar->txmgmt_idr_lock);
	if (buf_id < 0)
		return -ENOSPC;

	paddr = dma_map_single(ab->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(ab->dev, paddr)) {
		ath11k_warn(ab, "failed to DMA map mgmt Tx buffer\n");
		ret = -EIO;
		goto err_free_idr;
	}

	ATH11K_SKB_CB(skb)->paddr = paddr;

	ret = ath11k_wmi_mgmt_send(ar, arvif->vdev_id, buf_id, skb);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send mgmt frame: %d\n", ret);
		goto err_unmap_buf;
	}

	return 0;

err_unmap_buf:
	dma_unmap_single(ab->dev, ATH11K_SKB_CB(skb)->paddr,
			 skb->len, DMA_TO_DEVICE);
err_free_idr:
	spin_lock_bh(&ar->txmgmt_idr_lock);
	idr_remove(&ar->txmgmt_idr, buf_id);
	spin_unlock_bh(&ar->txmgmt_idr_lock);

	return ret;
}

void ath11k_mgmt_over_wmi_tx_purge(struct ath11k *ar)
{
	struct sk_buff *skb;

	while ((skb = skb_dequeue(&ar->wmi_mgmt_tx_queue)) != NULL)
		ieee80211_free_txskb(ar->hw, skb);
}

void ath11k_mgmt_over_wmi_tx_work(struct work_struct *work)
{
	struct ath11k *ar = container_of(work, struct ath11k, wmi_mgmt_tx_work);
	struct ieee80211_tx_info *info;
	struct ath11k_vif *arvif;
	struct sk_buff *skb;
	int ret;

	while ((skb = skb_dequeue(&ar->wmi_mgmt_tx_queue)) != NULL) {
		info = IEEE80211_SKB_CB(skb);
		arvif = ath11k_vif_to_arvif(info->control.vif);

		ret = ath11k_mac_mgmt_tx_wmi(ar, arvif, skb);
		if (ret) {
			ath11k_warn(ar->ab, "failed to transmit management frame %d\n",
				    ret);
			ieee80211_free_txskb(ar->hw, skb);
		}
	}
}

static int ath11k_mac_mgmt_tx(struct ath11k *ar, struct sk_buff *skb)
{
	struct sk_buff_head *q = &ar->wmi_mgmt_tx_queue;

	if (skb_queue_len(q) == ATH11K_TX_MGMT_NUM_PENDING_MAX) {
		ath11k_warn(ar->ab, "mgmt tx queue is full\n");
		return -ENOSPC;
	}

	skb_queue_tail(q, skb);
	ieee80211_queue_work(ar->hw, &ar->wmi_mgmt_tx_work);

	return 0;
}

static void ath11k_mac_op_tx(struct ieee80211_hw *hw,
			     struct ieee80211_tx_control *control,
			     struct sk_buff *skb)
{
	struct ath11k *ar = hw->priv;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_vif *vif = info->control.vif;
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	int ret;

	if (ieee80211_is_mgmt(hdr->frame_control)) {
		ret = ath11k_mac_mgmt_tx(ar, skb);
		if (ret) {
			ath11k_warn(ar->ab, "failed to queue management frame %d\n",
				    ret);
			ieee80211_free_txskb(ar->hw, skb);
		}
		return;
	}

	ret = ath11k_dp_tx(ar, arvif, skb);
	if (ret) {
		ath11k_warn(ar->ab, "failed to transmit frame %d\n", ret);
		ieee80211_free_txskb(ar->hw, skb);
	}
}

void ath11k_drain_tx(struct ath11k *ar)
{
	/* make sure rcu-protected mac80211 tx path itself is drained */
	synchronize_net();

	cancel_work_sync(&ar->wmi_mgmt_tx_work);
	ath11k_mgmt_over_wmi_tx_purge(ar);
}

static int ath11k_start(struct ieee80211_hw *hw)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_base *ab = ar->ab;
	struct ath11k_pdev *pdev = ar->pdev;
	int ret;

	mutex_lock(&ar->conf_mutex);

	switch (ar->state) {
	case ATH11K_STATE_OFF:
		ar->state = ATH11K_STATE_ON;
		break;
	case ATH11K_STATE_ON:
		WARN_ON(1);
		ret = -EINVAL;
		goto err;
	};

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_PMF_QOS,
					1, pdev->pdev_id);

	if (ret) {
		ath11k_err(ar->ab, "failed to enable PMF QOS: (%d\n", ret);
		goto err;
	}

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DYNAMIC_BW, 0,
					pdev->pdev_id);
	if (ret) {
		ath11k_err(ar->ab, "failed to enable dynamic bw: %d\n", ret);
		goto err;
	}

	INIT_LIST_HEAD(&ar->ppdu_stats_info);

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ARP_AC_OVERRIDE,
					0, pdev->pdev_id);
	if (ret) {
		ath11k_err(ab, "failed to set ac override for ARP: %d\n",
			   ret);
		goto err;
	}

	ret = ath11k_dp_htt_h2t_ppdu_stats_req(ar, HTT_PPDU_STATS_TAG_DEFAULT);
	if (ret) {
		ath11k_err(ab, "failed to req ppdu stats: %d\n", ret);
		goto err;
	}

	__ath11k_set_antenna(ar, ar->cfg_tx_chainmask, ar->cfg_rx_chainmask);

	/* TODO: Do we need to enable ANI? */

	ath11k_reg_update_chan_list(ar);

	ar->num_started_vdevs = 0;
	ar->num_created_vdevs = 0;

	mutex_unlock(&ar->conf_mutex);

	rcu_assign_pointer(ab->pdevs_active[ar->pdev_idx],
			   &ab->pdevs[ar->pdev_idx]);

	return 0;

err:
	ar->state = ATH11K_STATE_OFF;
	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static void ath11k_stop(struct ieee80211_hw *hw)
{
	struct ath11k *ar = hw->priv;

	ath11k_drain_tx(ar);

	mutex_lock(&ar->conf_mutex);
	ar->state = ATH11K_STATE_OFF;
	mutex_unlock(&ar->conf_mutex);

	cancel_delayed_work_sync(&ar->scan.timeout);
	cancel_work_sync(&ar->regd_update_work);

	rcu_assign_pointer(ar->ab->pdevs_active[ar->pdev_idx], NULL);

	synchronize_rcu();
}

static void
ath11k_mac_setup_vdev_create_params(struct ath11k_vif *arvif,
				    struct vdev_create_params *params)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_pdev *pdev = ar->pdev;

	params->if_id = arvif->vdev_id;
	params->type = arvif->vdev_type;
	params->subtype = arvif->vdev_subtype;
	params->pdev_id = pdev->pdev_id;

	if (pdev->cap.supported_bands & WMI_HOST_WLAN_2G_CAP) {
		params->chains[NL80211_BAND_2GHZ].tx = ar->num_tx_chains;
		params->chains[NL80211_BAND_2GHZ].rx = ar->num_rx_chains;
	}
	if (pdev->cap.supported_bands & WMI_HOST_WLAN_5G_CAP) {
		params->chains[NL80211_BAND_5GHZ].tx = ar->num_tx_chains;
		params->chains[NL80211_BAND_5GHZ].rx = ar->num_rx_chains;
	}
}

static int ath11k_add_interface(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_base *ab = ar->ab;
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	struct vdev_create_params vdev_param = {0};
	struct peer_create_params peer_param;
	u32 param_id, param_value;
	u16 nss;
	int i;
	int ret;
	int bit;

	vif->driver_flags |= IEEE80211_VIF_SUPPORTS_UAPSD;

	mutex_lock(&ar->conf_mutex);

	if (ar->num_peers >= TARGET_NUM_PEERS) {
		ath11k_warn(ab, "failed to create vdev due to insufficient peer entry resource in firmware\n");
		ret = -ENOBUFS;
		goto err;
	}

	if (ar->num_created_vdevs++ > TARGET_NUM_VDEVS) {
		ath11k_warn(ab, "failed to create vdev, reached max vdev limit %d\n",
			    TARGET_NUM_VDEVS);
		ret = -EBUSY;
		goto err;
	}

	memset(arvif, 0, sizeof(*arvif));

	arvif->ar = ar;
	arvif->vif = vif;

	INIT_LIST_HEAD(&arvif->list);

	/* TODO: Pending ap_csa_work implementation */

	/* Should we initialize any worker to handle connection loss indication
	 * from firmware in sta mode?
	 */

	for (i = 0; i < ARRAY_SIZE(arvif->bitrate_mask.control); i++) {
		arvif->bitrate_mask.control[i].legacy = 0xffffffff;
		memset(arvif->bitrate_mask.control[i].ht_mcs, 0xff,
		       sizeof(arvif->bitrate_mask.control[i].ht_mcs));
		memset(arvif->bitrate_mask.control[i].vht_mcs, 0xff,
		       sizeof(arvif->bitrate_mask.control[i].vht_mcs));
	}

	bit = __ffs64(ab->free_vdev_map);

	arvif->vdev_id = bit;
	arvif->vdev_subtype = WMI_VDEV_SUBTYPE_NONE;

	switch (vif->type) {
	case NL80211_IFTYPE_UNSPECIFIED:
	case NL80211_IFTYPE_STATION:
		arvif->vdev_type = WMI_VDEV_TYPE_STA;
		break;
	case NL80211_IFTYPE_AP:
		arvif->vdev_type = WMI_VDEV_TYPE_AP;
		break;
	default:
		WARN_ON(1);
		break;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac add interface id %d type %d subtype %d map %llx\n",
		   arvif->vdev_id, arvif->vdev_type, arvif->vdev_subtype,
		   ab->free_vdev_map);

	vif->cab_queue = arvif->vdev_id % (ATH11K_HW_MAX_QUEUES - 1);
	for (i = 0; i < ARRAY_SIZE(vif->hw_queue); i++)
		vif->hw_queue[i] = i % (ATH11K_HW_MAX_QUEUES - 1);

	ath11k_mac_setup_vdev_create_params(arvif, &vdev_param);

	ret = ath11k_wmi_vdev_create(ar, vif->addr, &vdev_param);
	if (ret) {
		ath11k_warn(ab, "failed to create WMI vdev %d: %d\n",
			    arvif->vdev_id, ret);
		goto err;
	}

	ab->free_vdev_map &= ~(1LL << arvif->vdev_id);
	spin_lock_bh(&ar->data_lock);
	list_add(&arvif->list, &ar->arvifs);
	spin_unlock_bh(&ar->data_lock);

	arvif->def_wep_key_index = -1;
	param_id = WMI_VDEV_PARAM_DEF_KEYID;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    param_id, arvif->def_wep_key_index);
	if (ret)
		ath11k_warn(ar->ab, "Failed to set default keyid: %d\n", ret);

	param_id = WMI_VDEV_PARAM_TX_ENCAP_TYPE;
	param_value = ATH11K_HW_TXRX_NATIVE_WIFI;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    param_id, param_value);
	if (ret) {
		ath11k_warn(ab, "failed to set vdev %d tx encap mode: %d\n",
			    arvif->vdev_id, ret);
		goto err_vdev_del;
	}

	nss = get_num_chains(ar->cfg_tx_chainmask) ? : 1;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_NSS, nss);
	if (ret) {
		ath11k_warn(ab, "failed to set vdev %d chainmask 0x%x, nss %d :%d\n",
			    arvif->vdev_id, ar->cfg_tx_chainmask, nss, ret);
		goto err_vdev_del;
	}

	if (arvif->vdev_type == WMI_VDEV_TYPE_AP) {
		peer_param.vdev_id = arvif->vdev_id;
		peer_param.peer_addr = vif->addr;
		peer_param.peer_type = WMI_PEER_TYPE_DEFAULT;
		ret = ath11k_peer_create(ar, arvif, NULL, &peer_param);
		if (ret) {
			ath11k_warn(ab, "failed to vdev %d create peer for AP: %d\n",
				    arvif->vdev_id, ret);
			goto err_vdev_del;
		}
	}

	if (arvif->vdev_type == WMI_VDEV_TYPE_AP) {
		ret = ath11k_mac_set_kickout(arvif);
		if (ret) {
			ath11k_warn(ar->ab, "failed to set vdev %i kickout parameters: %d\n",
				    arvif->vdev_id, ret);
			goto err_peer_del;
		}
	}

	if (arvif->vdev_type == WMI_VDEV_TYPE_STA) {
		param_id = WMI_STA_PS_PARAM_RX_WAKE_POLICY;
		param_value = WMI_STA_PS_RX_WAKE_POLICY_WAKE;
		ret = ath11k_wmi_set_sta_ps_param(ar, arvif->vdev_id,
						  param_id, param_value);
		if (ret) {
			ath11k_warn(ar->ab, "failed to set vdev %d RX wake policy: %d\n",
				    arvif->vdev_id, ret);
			goto err_peer_del;
		}

		param_id = WMI_STA_PS_PARAM_TX_WAKE_THRESHOLD;
		param_value = WMI_STA_PS_TX_WAKE_THRESHOLD_ALWAYS;
		ret = ath11k_wmi_set_sta_ps_param(ar, arvif->vdev_id,
						  param_id, param_value);
		if (ret) {
			ath11k_warn(ar->ab, "failed to set vdev %d TX wake threshold: %d\n",
				    arvif->vdev_id, ret);
			goto err_peer_del;
		}

		param_id = WMI_STA_PS_PARAM_PSPOLL_COUNT;
		param_value = WMI_STA_PS_PSPOLL_COUNT_NO_MAX;
		ret = ath11k_wmi_set_sta_ps_param(ar, arvif->vdev_id,
						  param_id, param_value);
		if (ret) {
			ath11k_warn(ar->ab, "failed to set vdev %d pspoll count: %d\n",
				    arvif->vdev_id, ret);
			goto err_peer_del;
		}
	}

	arvif->txpower = vif->bss_conf.txpower;
	ret = ath11k_mac_txpower_recalc(ar);
	if (ret)
		goto err_peer_del;

	param_id = WMI_VDEV_PARAM_RTS_THRESHOLD;
	param_value = ar->hw->wiphy->rts_threshold;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    param_id, param_value);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set rts threshold for vdev %d: %d\n",
			    arvif->vdev_id, ret);
	}

	ret = ath11k_mac_set_txbf_conf(arvif);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set txbf conf for vdev %d: %d\n",
			    arvif->vdev_id, ret);
	}

	ath11k_dp_vdev_tx_attach(ar, arvif);

	mutex_unlock(&ar->conf_mutex);

	return 0;

err_peer_del:
	if (arvif->vdev_type == WMI_VDEV_TYPE_AP)
		ath11k_wmi_send_peer_delete_cmd(ar, vif->addr, arvif->vdev_id);

err_vdev_del:
	ath11k_wmi_vdev_delete(ar, arvif->vdev_id);
	ab->free_vdev_map |= 1LL << arvif->vdev_id;
	spin_lock_bh(&ar->data_lock);
	list_del(&arvif->list);
	spin_unlock_bh(&ar->data_lock);

err:
	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static int ath11k_mac_vif_unref(int buf_id, void *skb, void *ctx)
{
	struct ieee80211_vif *vif = (struct ieee80211_vif *)ctx;
	struct ath11k_skb_cb *skb_cb = ATH11K_SKB_CB((struct sk_buff *)skb);

	if (skb_cb->vif == vif)
		skb_cb->vif = NULL;

	return 0;
}

static void ath11k_remove_interface(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	struct ath11k_base *ab = ar->ab;
	int ret;
	int i;

	/* TODO: Pending ap_csa_work implementation */

	mutex_lock(&ar->conf_mutex);

	ath11k_dbg(ab, ATH11K_DBG_MAC, "mac remove interface (vdev %d)\n",
		   arvif->vdev_id);

	ab->free_vdev_map |= 1LL << (arvif->vdev_id);
	spin_lock_bh(&ar->data_lock);
	list_del(&arvif->list);
	spin_unlock_bh(&ar->data_lock);

	if (arvif->vdev_type == WMI_VDEV_TYPE_AP) {
		ath11k_dp_peer_cleanup(ar, arvif->vdev_id, vif->addr);
		ret = ath11k_peer_delete(ar, arvif->vdev_id, vif->addr);
		if (ret)
			ath11k_warn(ab, "failed to submit AP self-peer removal on vdev %d: %d\n",
				    arvif->vdev_id, ret);
	}

	spin_lock_bh(&ar->data_lock);
	ar->num_peers--;
	spin_unlock_bh(&ar->data_lock);

	ret = ath11k_wmi_vdev_delete(ar, arvif->vdev_id);
	if (ret)
		ath11k_warn(ab, "failed to delete WMI vdev %d: %d\n",
			    arvif->vdev_id, ret);

	ar->num_created_vdevs--;

	ath11k_peer_cleanup(ar, arvif->vdev_id);

	idr_for_each(&ar->txmgmt_idr,
		     ath11k_mac_vif_txmgmt_idr_remove, vif);

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++) {
		spin_lock_bh(&ab->dp.tx_ring[i].tx_idr_lock);
		idr_for_each(&ab->dp.tx_ring[i].txbuf_idr,
			     ath11k_mac_vif_unref, vif);
		spin_unlock_bh(&ab->dp.tx_ring[i].tx_idr_lock);
	}

	/* Recalc txpower for remaining vdev */
	ath11k_mac_txpower_recalc(ar);

	/* TODO: recal traffic pause state based on the available vdevs */

	mutex_unlock(&ar->conf_mutex);
}

/* FIXME: Has to be verified. */
#define SUPPORTED_FILTERS			\
	(FIF_ALLMULTI |				\
	FIF_CONTROL |				\
	FIF_PSPOLL |				\
	FIF_OTHER_BSS |				\
	FIF_BCN_PRBRESP_PROMISC |		\
	FIF_PROBE_REQ |				\
	FIF_FCSFAIL)

static void ath11k_configure_filter(struct ieee80211_hw *hw,
				    unsigned int changed_flags,
				    unsigned int *total_flags,
				    u64 multicast)
{
	struct ath11k *ar = hw->priv;

	mutex_lock(&ar->conf_mutex);

	changed_flags &= SUPPORTED_FILTERS;
	*total_flags &= SUPPORTED_FILTERS;
	ar->filter_flags = *total_flags;

	/* TODO: Send filter configuration to target as appropriate */

	mutex_unlock(&ar->conf_mutex);
}

static int ath11k_get_antenna(struct ieee80211_hw *hw, u32 *tx_ant, u32 *rx_ant)
{
	struct ath11k *ar = hw->priv;

	mutex_lock(&ar->conf_mutex);

	*tx_ant = ar->cfg_tx_chainmask;
	*rx_ant = ar->cfg_rx_chainmask;

	mutex_unlock(&ar->conf_mutex);

	return 0;
}

static int ath11k_set_antenna(struct ieee80211_hw *hw, u32 tx_ant, u32 rx_ant)
{
	struct ath11k *ar = hw->priv;
	int ret;

	mutex_lock(&ar->conf_mutex);
	ret = __ath11k_set_antenna(ar, tx_ant, rx_ant);
	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static int ath11k_ampdu_action(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       struct ieee80211_ampdu_params *params)
{
	struct ath11k *ar = hw->priv;
	int ret = -EINVAL;

	mutex_lock(&ar->conf_mutex);

	switch (params->action) {
	case IEEE80211_AMPDU_RX_START:
		ret = ath11k_dp_rx_ampdu_start(ar, params);
		break;
	case IEEE80211_AMPDU_RX_STOP:
		ret = ath11k_dp_rx_ampdu_stop(ar, params);
		break;
	case IEEE80211_AMPDU_TX_START:
	case IEEE80211_AMPDU_TX_STOP_CONT:
	case IEEE80211_AMPDU_TX_STOP_FLUSH:
	case IEEE80211_AMPDU_TX_STOP_FLUSH_CONT:
	case IEEE80211_AMPDU_TX_OPERATIONAL:
		/* Tx A-MPDU aggregation offloaded to hw/fw so deny mac80211
		 * Tx aggregation requests.
		 */
		ret = -EOPNOTSUPP;
		break;
	}

	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static int ath11k_mac_op_add_chanctx(struct ieee80211_hw *hw,
				     struct ieee80211_chanctx_conf *ctx)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_base *ab = ar->ab;

	ath11k_dbg(ab, ATH11K_DBG_MAC,
		   "mac chanctx add freq %hu width %d ptr %pK\n",
		   ctx->def.chan->center_freq, ctx->def.width, ctx);

	mutex_lock(&ar->conf_mutex);

	spin_lock_bh(&ar->data_lock);
	/* TODO: In case of multiple channel context, populate rx_channel from
	 * Rx PPDU desc information.
	 */
	ar->rx_channel = ctx->def.chan;
	spin_unlock_bh(&ar->data_lock);

	/* TODO: CAC */

	mutex_unlock(&ar->conf_mutex);

	return 0;
}

static void ath11k_mac_op_remove_chanctx(struct ieee80211_hw *hw,
					 struct ieee80211_chanctx_conf *ctx)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_base *ab = ar->ab;

	ath11k_dbg(ab, ATH11K_DBG_MAC,
		   "mac chanctx remove freq %hu width %d ptr %pK\n",
		   ctx->def.chan->center_freq, ctx->def.width, ctx);

	mutex_lock(&ar->conf_mutex);

	spin_lock_bh(&ar->data_lock);
	/* TODO: In case of there is one more channel context left, populate
	 * rx_channel with the channel of that remaining channel context.
	 */
	ar->rx_channel = NULL;
	spin_unlock_bh(&ar->data_lock);

	/* TODO: CAC */

	mutex_unlock(&ar->conf_mutex);
}

static inline int ath11k_mac_vdev_setup_sync(struct ath11k *ar)
{
	lockdep_assert_held(&ar->conf_mutex);

	if (!wait_for_completion_timeout(&ar->vdev_setup_done,
					 ATH11K_VDEV_SETUP_TIMEOUT_HZ))
		return -ETIMEDOUT;

	return ar->last_wmi_vdev_start_status ? -EINVAL : 0;
}

static int
ath11k_mac_vdev_start_restart(struct ath11k_vif *arvif,
			      const struct cfg80211_chan_def *chandef,
			      bool restart)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;
	struct wmi_vdev_start_req_arg arg = {};
	int ret = 0;

	lockdep_assert_held(&ar->conf_mutex);

	reinit_completion(&ar->vdev_setup_done);

	arg.vdev_id = arvif->vdev_id;
	arg.dtim_period = arvif->dtim_period;
	arg.bcn_intval = arvif->beacon_interval;

	arg.channel.freq = chandef->chan->center_freq;
	arg.channel.band_center_freq1 = chandef->center_freq1;
	arg.channel.band_center_freq2 = chandef->center_freq2;
	arg.channel.mode = chan_to_phymode(chandef);

	arg.channel.min_power = 0;
	arg.channel.max_power = chandef->chan->max_power * 2;
	arg.channel.max_reg_power = chandef->chan->max_reg_power * 2;
	arg.channel.max_antenna_gain = chandef->chan->max_antenna_gain * 2;

	if (arvif->vdev_type == WMI_VDEV_TYPE_AP) {
		arg.ssid = arvif->u.ap.ssid;
		arg.ssid_len = arvif->u.ap.ssid_len;
		arg.hidden_ssid = arvif->u.ap.hidden_ssid;

		/* For now allow DFS for AP mode */
		arg.channel.chan_radar =
			!!(chandef->chan->flags & IEEE80211_CHAN_RADAR);

		/* TODO: Notify if secondary 80Mhz also needs radar detection */
	}

	ath11k_dbg(ab, ATH11K_DBG_MAC,
		   "mac vdev %d start center_freq %d phymode %s\n",
		   arg.vdev_id, arg.channel.freq,
		   ath11k_wmi_phymode_str(arg.channel.mode));

	ret = ath11k_wmi_vdev_start(ar, &arg, restart);
	if (ret) {
		ath11k_warn(ar->ab, "failed to %s WMI vdev %i\n",
			    restart ? "restart" : "start", arg.vdev_id);
		return ret;
	}

	ret = ath11k_mac_vdev_setup_sync(ar);
	if (ret) {
		ath11k_warn(ab, "failed to synchronize setup for vdev %i %s: %d\n",
			    arg.vdev_id, restart ? "restart" : "start", ret);
		return ret;
	}

	ar->num_started_vdevs++;

	/* TODO: Recalc radar */

	return 0;
}

static int ath11k_mac_vdev_stop(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	reinit_completion(&ar->vdev_setup_done);

	ret = ath11k_wmi_vdev_stop(ar, arvif->vdev_id);
	if (ret) {
		ath11k_warn(ar->ab, "failed to stop WMI vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	ret = ath11k_mac_vdev_setup_sync(ar);
	if (ret) {
		ath11k_warn(ar->ab, "failed to synchronize setup for vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	WARN_ON(ar->num_started_vdevs == 0);

	ar->num_started_vdevs--;

	/* TODO: Recalc radar */

	return 0;
}

static int ath11k_mac_vdev_start(struct ath11k_vif *arvif,
				 const struct cfg80211_chan_def *chandef)
{
	return ath11k_mac_vdev_start_restart(arvif, chandef, false);
}

static int ath11k_mac_vdev_restart(struct ath11k_vif *arvif,
				   const struct cfg80211_chan_def *chandef)
{
	return ath11k_mac_vdev_start_restart(arvif, chandef, true);
}

struct ath11k_mac_change_chanctx_arg {
	struct ieee80211_chanctx_conf *ctx;
	struct ieee80211_vif_chanctx_switch *vifs;
	int n_vifs;
	int next_vif;
};

static void
ath11k_mac_change_chanctx_cnt_iter(void *data, u8 *mac,
				   struct ieee80211_vif *vif)
{
	struct ath11k_mac_change_chanctx_arg *arg = data;

	if (rcu_access_pointer(vif->chanctx_conf) != arg->ctx)
		return;

	arg->n_vifs++;
}

static void
ath11k_mac_change_chanctx_fill_iter(void *data, u8 *mac,
				    struct ieee80211_vif *vif)
{
	struct ath11k_mac_change_chanctx_arg *arg = data;
	struct ieee80211_chanctx_conf *ctx;

	ctx = rcu_access_pointer(vif->chanctx_conf);
	if (ctx != arg->ctx)
		return;

	if (WARN_ON(arg->next_vif == arg->n_vifs))
		return;

	arg->vifs[arg->next_vif].vif = vif;
	arg->vifs[arg->next_vif].old_ctx = ctx;
	arg->vifs[arg->next_vif].new_ctx = ctx;
	arg->next_vif++;
}

static void
ath11k_mac_update_vif_chan(struct ath11k *ar,
			   struct ieee80211_vif_chanctx_switch *vifs,
			   int n_vifs)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_vif *arvif;
	int ret;
	int i;

	lockdep_assert_held(&ar->conf_mutex);

	for (i = 0; i < n_vifs; i++) {
		arvif = (void *)vifs[i].vif->drv_priv;

		ath11k_dbg(ab, ATH11K_DBG_MAC,
			   "mac chanctx switch vdev_id %i freq %hu->%hu width %d->%d\n",
			   arvif->vdev_id,
			   vifs[i].old_ctx->def.chan->center_freq,
			   vifs[i].new_ctx->def.chan->center_freq,
			   vifs[i].old_ctx->def.width,
			   vifs[i].new_ctx->def.width);

		if (WARN_ON(!arvif->is_started))
			continue;

		if (WARN_ON(!arvif->is_up))
			continue;

		ret = ath11k_wmi_vdev_down(ar, arvif->vdev_id);
		if (ret) {
			ath11k_warn(ab, "failed to down vdev %d: %d\n",
				    arvif->vdev_id, ret);
			continue;
		}
	}

	/* All relevant vdevs are downed and associated channel resources
	 * should be available for the channel switch now.
	 */

	/* TODO: Update ar->rx_channel */

	for (i = 0; i < n_vifs; i++) {
		arvif = (void *)vifs[i].vif->drv_priv;

		if (WARN_ON(!arvif->is_started))
			continue;

		if (WARN_ON(!arvif->is_up))
			continue;

		ret = ath11k_mac_setup_bcn_tmpl(arvif);
		if (ret)
			ath11k_warn(ab, "failed to update bcn tmpl during csa: %d\n",
				    ret);

		ret = ath11k_mac_setup_prb_tmpl(arvif);
		if (ret)
			ath11k_warn(ar->ab, "failed to update prb tmpl: %d\n",
				    ret);

		ret = ath11k_mac_vdev_restart(arvif, &vifs[i].new_ctx->def);
		if (ret) {
			ath11k_warn(ab, "failed to restart vdev %d: %d\n",
				    arvif->vdev_id, ret);
			continue;
		}

		ret = ath11k_wmi_vdev_up(arvif->ar, arvif->vdev_id, arvif->aid,
					 arvif->bssid);
		if (ret) {
			ath11k_warn(ab, "failed to bring vdev up %d: %d\n",
				    arvif->vdev_id, ret);
			continue;
		}
	}
}

static void
ath11k_mac_update_active_vif_chan(struct ath11k *ar,
				  struct ieee80211_chanctx_conf *ctx)
{
	struct ath11k_mac_change_chanctx_arg arg = { .ctx = ctx };

	lockdep_assert_held(&ar->conf_mutex);

	ieee80211_iterate_active_interfaces_atomic(ar->hw,
					IEEE80211_IFACE_ITER_NORMAL,
					ath11k_mac_change_chanctx_cnt_iter,
					&arg);
	if (arg.n_vifs == 0)
		return;

	arg.vifs = kcalloc(arg.n_vifs, sizeof(arg.vifs[0]), GFP_KERNEL);
	if (!arg.vifs)
		return;

	ieee80211_iterate_active_interfaces_atomic(ar->hw,
					IEEE80211_IFACE_ITER_NORMAL,
					ath11k_mac_change_chanctx_fill_iter,
					&arg);

	ath11k_mac_update_vif_chan(ar, arg.vifs, arg.n_vifs);

	kfree(arg.vifs);
}

static void ath11k_mac_op_change_chanctx(struct ieee80211_hw *hw,
					 struct ieee80211_chanctx_conf *ctx,
					 u32 changed)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_base *ab = ar->ab;

	mutex_lock(&ar->conf_mutex);

	ath11k_dbg(ab, ATH11K_DBG_MAC,
		   "mac chanctx change freq %hu width %d ptr %pK changed %x\n",
		   ctx->def.chan->center_freq, ctx->def.width, ctx, changed);

	/* This shouldn't really happen because channel switching should use
	 * switch_vif_chanctx().
	 */
	if (WARN_ON(changed & IEEE80211_CHANCTX_CHANGE_CHANNEL))
		goto unlock;

	if (changed & IEEE80211_CHANCTX_CHANGE_WIDTH)
		ath11k_mac_update_active_vif_chan(ar, ctx);

	/* TODO: Recalc radar detection */

unlock:
	mutex_unlock(&ar->conf_mutex);
}

static int
ath11k_mac_op_assign_vif_chanctx(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 struct ieee80211_chanctx_conf *ctx)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_base *ab = ar->ab;
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	int ret;

	mutex_lock(&ar->conf_mutex);

	ath11k_dbg(ab, ATH11K_DBG_MAC,
		   "mac chanctx assign ptr %pK vdev_id %i\n",
		   ctx, arvif->vdev_id);

	if (WARN_ON(arvif->is_started)) {
		mutex_unlock(&ar->conf_mutex);
		return -EBUSY;
	}

	ret = ath11k_mac_vdev_start(arvif, &ctx->def);
	if (ret) {
		ath11k_warn(ab, "failed to start vdev %i addr %pM on freq %d: %d\n",
			    arvif->vdev_id, vif->addr,
			    ctx->def.chan->center_freq, ret);
		goto err;
	}

	arvif->is_started = true;

	/* TODO: Setup ps and cts/rts protection */

	mutex_unlock(&ar->conf_mutex);

	return 0;

err:
	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static void
ath11k_mac_op_unassign_vif_chanctx(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_chanctx_conf *ctx)
{
	struct ath11k *ar = hw->priv;
	struct ath11k_base *ab = ar->ab;
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	int ret;

	mutex_lock(&ar->conf_mutex);

	ath11k_dbg(ab, ATH11K_DBG_MAC,
		   "mac chanctx unassign ptr %pK vdev_id %i\n",
		   ctx, arvif->vdev_id);

	WARN_ON(!arvif->is_started);

	ret = ath11k_mac_vdev_stop(arvif);
	if (ret)
		ath11k_warn(ab, "failed to stop vdev %i: %d\n",
			    arvif->vdev_id, ret);

	arvif->is_started = false;

	mutex_unlock(&ar->conf_mutex);
}

static int
ath11k_mac_op_switch_vif_chanctx(struct ieee80211_hw *hw,
				 struct ieee80211_vif_chanctx_switch *vifs,
				 int n_vifs,
				 enum ieee80211_chanctx_switch_mode mode)
{
	struct ath11k *ar = hw->priv;

	mutex_lock(&ar->conf_mutex);

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC,
		   "mac chanctx switch n_vifs %d mode %d\n",
		   n_vifs, mode);
	ath11k_mac_update_vif_chan(ar, vifs, n_vifs);

	mutex_unlock(&ar->conf_mutex);

	return 0;
}

static int
ath11k_set_vdev_param_to_all_vifs(struct ath11k *ar, int param, u32 value)
{
	struct ath11k_vif *arvif;
	int ret = 0;

	mutex_lock(&ar->conf_mutex);
	list_for_each_entry(arvif, &ar->arvifs, list) {
		ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "setting mac vdev %d param %d value %d\n",
			   param, arvif->vdev_id, value);

		ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    param, value);
		if (ret) {
			ath11k_warn(ar->ab, "failed to set param %d for vdev %d: %d\n",
				    param, arvif->vdev_id, ret);
			break;
		}
	}
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

/* mac80211 stores device specific RTS/Fragmentation threshold value,
 * this is set interface specific to firmware from ath11k driver
 */
static int ath11k_set_rts_threshold(struct ieee80211_hw *hw, u32 value)
{
	struct ath11k *ar = hw->priv;
	int param_id = WMI_VDEV_PARAM_RTS_THRESHOLD;

	return ath11k_set_vdev_param_to_all_vifs(ar, param_id, value);
}

static int ath11k_set_frag_threshold(struct ieee80211_hw *hw, u32 value)
{
	struct ath11k *ar = hw->priv;
	int param_id = WMI_VDEV_PARAM_FRAGMENTATION_THRESHOLD;

	return ath11k_set_vdev_param_to_all_vifs(ar, param_id, value);
}

static void ath11k_flush(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			 u32 queues, bool drop)
{
	struct ath11k *ar = hw->priv;
	long time_left;

	if (drop)
		return;

	time_left = wait_event_timeout(ar->dp.tx_empty_waitq,
				       (atomic_read(&ar->dp.num_tx_pending) == 0),
				       ATH1K_FLUSH_TIMEOUT);
	if (time_left == 0)
		ath11k_warn(ar->ab, "failed to flush transmit queue %ld\n", time_left);
}

static int
ath11k_mac_bitrate_mask_num_ht_rates(struct ath11k *ar,
				     enum nl80211_band band,
				     const struct cfg80211_bitrate_mask *mask)
{
	int num_rates = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].ht_mcs); i++)
		num_rates += hweight16(mask->control[band].ht_mcs[i]);

	return num_rates;
}

static bool
ath11k_mac_has_single_legacy_rate(struct ath11k *ar,
				  enum nl80211_band band,
				  const struct cfg80211_bitrate_mask *mask)
{
	int num_rates = 0;

	num_rates += hweight32(mask->control[band].legacy);

	if (ath11k_mac_bitrate_mask_num_ht_rates(ar, band, mask))
		return false;

	if (ath11k_mac_bitrate_mask_num_vht_rates(ar, band, mask))
		return false;

	return num_rates == 1;
}

static bool
ath11k_mac_bitrate_mask_get_single_nss(struct ath11k *ar,
				       enum nl80211_band band,
				       const struct cfg80211_bitrate_mask *mask,
				       int *nss)
{
	struct ieee80211_supported_band *sband = &ar->mac.sbands[band];
	u16 vht_mcs_map = le16_to_cpu(sband->vht_cap.vht_mcs.tx_mcs_map);
	u8 ht_nss_mask = 0;
	u8 vht_nss_mask = 0;
	int i;

	/* No need to consider legacy here. Basic rates are always present
	 * in bitrate mask
	 */

	for (i = 0; i < ARRAY_SIZE(mask->control[band].ht_mcs); i++) {
		if (mask->control[band].ht_mcs[i] == 0)
			continue;
		else if (mask->control[band].ht_mcs[i] ==
			 sband->ht_cap.mcs.rx_mask[i])
			ht_nss_mask |= BIT(i);
		else
			return false;
	}

	for (i = 0; i < ARRAY_SIZE(mask->control[band].vht_mcs); i++) {
		if (mask->control[band].vht_mcs[i] == 0)
			continue;
		else if (mask->control[band].vht_mcs[i] ==
			 ath11k_mac_get_max_vht_mcs_map(vht_mcs_map, i))
			vht_nss_mask |= BIT(i);
		else
			return false;
	}

	if (ht_nss_mask != vht_nss_mask)
		return false;

	if (ht_nss_mask == 0)
		return false;

	if (BIT(fls(ht_nss_mask)) - 1 != ht_nss_mask)
		return false;

	*nss = fls(ht_nss_mask);

	return true;
}

static int
ath11k_mac_get_single_legacy_rate(struct ath11k *ar,
				  enum nl80211_band band,
				  const struct cfg80211_bitrate_mask *mask,
				  u8 *rate, u8 *nss)
{
	int rate_idx;
	u16 bitrate;
	u8 preamble;
	u8 hw_rate;

	if (hweight32(mask->control[band].legacy) != 1)
		return -EINVAL;

	rate_idx = ffs(mask->control[band].legacy) - 1;

	if (band == NL80211_BAND_5GHZ)
		rate_idx += ATH11K_MAC_FIRST_OFDM_RATE_IDX;

	hw_rate = ath11k_legacy_rates[rate_idx].hw_value;
	bitrate = ath11k_legacy_rates[rate_idx].bitrate;

	if (ath11k_mac_bitrate_is_cck(bitrate))
		preamble = WMI_RATE_PREAMBLE_CCK;
	else
		preamble = WMI_RATE_PREAMBLE_OFDM;

	*nss = 1;
	*rate = ATH11K_HW_RATE_CODE(hw_rate, 0, preamble);

	return 0;
}

static int ath11k_mac_set_fixed_rate_params(struct ath11k_vif *arvif,
					    u8 rate, u8 nss, u8 sgi, u8 ldpc)
{
	struct ath11k *ar = arvif->ar;
	u32 vdev_param;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "mac set fixed rate params vdev %i rate 0x%02hhx nss %hhu sgi %hhu\n",
		   arvif->vdev_id, rate, nss, sgi);

	vdev_param = WMI_VDEV_PARAM_FIXED_RATE;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    vdev_param, rate);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set fixed rate param 0x%02x: %d\n",
			    rate, ret);
		return ret;
	}

	vdev_param = WMI_VDEV_PARAM_NSS;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    vdev_param, nss);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set nss param %d: %d\n",
			    nss, ret);
		return ret;
	}

	vdev_param = WMI_VDEV_PARAM_SGI;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    vdev_param, sgi);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set sgi param %d: %d\n",
			    sgi, ret);
		return ret;
	}

	vdev_param = WMI_VDEV_PARAM_LDPC;
	ret = ath11k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    vdev_param, ldpc);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set ldpc param %d: %d\n",
			    ldpc, ret);
		return ret;
	}

	return 0;
}

static bool
ath11k_mac_vht_mcs_range_present(struct ath11k *ar,
				 enum nl80211_band band,
				 const struct cfg80211_bitrate_mask *mask)
{
	int i;
	u16 vht_mcs;

	for (i = 0; i < NL80211_VHT_NSS_MAX; i++) {
		vht_mcs = mask->control[band].vht_mcs[i];

		switch (vht_mcs) {
		case 0:
		case BIT(8) - 1:
		case BIT(9) - 1:
		case BIT(10) - 1:
			break;
		default:
			return false;
		}
	}

	return true;
}

static void ath11k_mac_set_bitrate_mask_iter(void *data,
					     struct ieee80211_sta *sta)
{
	struct ath11k_vif *arvif = data;
	struct ath11k_sta *arsta = (struct ath11k_sta *)sta->drv_priv;
	struct ath11k *ar = arvif->ar;

	spin_lock_bh(&ar->data_lock);
	arsta->changed |= IEEE80211_RC_SUPP_RATES_CHANGED;
	spin_unlock_bh(&ar->data_lock);

	ieee80211_queue_work(ar->hw, &arsta->update_wk);
}

static int
ath11k_mac_op_set_bitrate_mask(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       const struct cfg80211_bitrate_mask *mask)
{
	struct ath11k_vif *arvif = (void *)vif->drv_priv;
	struct cfg80211_chan_def def;
	struct ath11k *ar = arvif->ar;
	enum nl80211_band band;
	const u8 *ht_mcs_mask;
	const u16 *vht_mcs_mask;
	u8 rate;
	u8 nss;
	u8 sgi;
	u8 ldpc;
	int single_nss;
	int ret;
	int num_rates;

	if (ath11k_mac_vif_chan(vif, &def))
		return -EPERM;

	band = def.chan->band;
	ht_mcs_mask = mask->control[band].ht_mcs;
	vht_mcs_mask = mask->control[band].vht_mcs;
	ldpc = !!(ar->ht_cap_info & WMI_HT_CAP_LDPC);

	sgi = mask->control[band].gi;
	if (sgi == NL80211_TXRATE_FORCE_LGI)
		return -EINVAL;

	/* mac80211 doesn't support sending a fixed HT/VHT MCS alone, rather it
	 * requires passing atleast one of used basic rates along with them.
	 * Fixed rate setting across different preambles(legacy, HT, VHT) is
	 * not supported by the FW. Hence use of FIXED_RATE vdev param is not
	 * suitable for setting single HT/VHT rates.
	 * But, there could be a single basic rate passed from userspace which
	 * can be done through the FIXED_RATE param.
	 */
	if (ath11k_mac_has_single_legacy_rate(ar, band, mask)) {
		ret = ath11k_mac_get_single_legacy_rate(ar, band, mask, &rate,
							&nss);
		if (ret) {
			ath11k_warn(ar->ab, "failed to get single legacy rate for vdev %i: %d\n",
				    arvif->vdev_id, ret);
			return ret;
		}
	} else if (ath11k_mac_bitrate_mask_get_single_nss(ar, band, mask,
							  &single_nss)) {
		rate = WMI_FIXED_RATE_NONE;
		nss = single_nss;
	} else {
		rate = WMI_FIXED_RATE_NONE;
		nss = min((u32)ar->num_tx_chains,
			  max(ath11k_mac_max_ht_nss(ht_mcs_mask),
			      ath11k_mac_max_vht_nss(vht_mcs_mask)));

		/* If multiple rates across different preambles are given
		 * we can reconfigure this info with all peers using PEER_ASSOC
		 * command with the below exception cases.
		 * - Single VHT Rate : peer_assoc command accommodates only MCS
		 * range values i.e 0-7, 0-8, 0-9 for VHT. Though mac80211
		 * mandates passing basic rates along with HT/VHT rates, FW
		 * doesn't allow switching from VHT to Legacy. Hence instead of
		 * setting legacy and VHT rates using RATEMASK_CMD vdev cmd,
		 * we could set this VHT rate as peer fixed rate param, which
		 * will override FIXED rate and FW rate control algorithm.
		 * If single VHT rate is passed along with HT rates, we select
		 * the VHT rate as fixed rate for vht peers.
		 * - Multiple VHT Rates : When Multiple VHT rates are given,this
		 * can be set using RATEMASK CMD which uses FW rate-ctl alg.
		 * TODO: Setting multiple VHT MCS and replacing peer_assoc with
		 * RATEMASK_CMDID can cover all use cases of setting rates
		 * across multiple preambles and rates within same type.
		 * But requires more validation of the command at this point.
		 */

		num_rates = ath11k_mac_bitrate_mask_num_vht_rates(ar, band,
								  mask);

		if (!ath11k_mac_vht_mcs_range_present(ar, band, mask) &&
		    num_rates > 1) {
			/* TODO: Handle multiple VHT MCS values setting using
			 * RATEMASK CMD
			 */
			ath11k_warn(ar->ab,
				    "Setting more than one MCS Value in bitrate mask not supported\n");
			return -EINVAL;
		}

		mutex_lock(&ar->conf_mutex);

		arvif->bitrate_mask = *mask;
		ieee80211_iterate_stations_atomic(ar->hw,
						  ath11k_mac_set_bitrate_mask_iter,
						  arvif);

		mutex_unlock(&ar->conf_mutex);
	}

	mutex_lock(&ar->conf_mutex);

	ret = ath11k_mac_set_fixed_rate_params(arvif, rate, nss, sgi, ldpc);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set fixed rate params on vdev %i: %d\n",
			    arvif->vdev_id, ret);
	}

	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static void
ath11k_mac_update_bss_chan_survey(struct ath11k *ar,
				  struct ieee80211_channel *channel)
{
	int ret;
	enum wmi_bss_chan_info_req_type type = WMI_BSS_SURVEY_REQ_TYPE_READ;

	lockdep_assert_held(&ar->conf_mutex);

	if (!test_bit(WMI_SERVICE_BSS_CHANNEL_INFO_64,
		      ar->ab->wmi_sc.svc_map) ||
	    (ar->rx_channel != channel))
		return;

	if (ar->scan.state != ATH11K_SCAN_IDLE) {
		ath11k_dbg(ar->ab, ATH11K_DBG_MAC,
			   "ignoring bss chan info req while scanning..\n");
		return;
	}

	reinit_completion(&ar->bss_survey_done);

	ret = ath11k_wmi_pdev_bss_chan_info_request(ar, type);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send pdev bss chan info request\n");
		return;
	}

	ret = wait_for_completion_timeout(&ar->bss_survey_done, 3 * HZ);
	if (ret == 0) {
		ath11k_warn(ar->ab, "bss channel survey timed out\n");
	}
}

static int ath11k_get_survey(struct ieee80211_hw *hw, int idx,
			     struct survey_info *survey)
{
	struct ath11k *ar = hw->priv;
	struct ieee80211_supported_band *sband;
	struct survey_info *ar_survey;
	int ret = 0;

	if (idx >= ATH11K_NUM_CHANS)
		return -ENOENT;

	ar_survey = &ar->survey[idx];

	mutex_lock(&ar->conf_mutex);

	sband = hw->wiphy->bands[NL80211_BAND_2GHZ];
	if (sband && idx >= sband->n_channels) {
		idx -= sband->n_channels;
		sband = NULL;
	}

	if (!sband)
		sband = hw->wiphy->bands[NL80211_BAND_5GHZ];

	if (!sband || idx >= sband->n_channels) {
		ret = -ENOENT;
		goto exit;
	}

	ath11k_mac_update_bss_chan_survey(ar, &sband->channels[idx]);

	spin_lock_bh(&ar->data_lock);
	memcpy(survey, ar_survey, sizeof(*survey));
	spin_unlock_bh(&ar->data_lock);

	survey->channel = &sband->channels[idx];

	if (ar->rx_channel == survey->channel)
		survey->filled |= SURVEY_INFO_IN_USE;

exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

void ath11k_mac_fill_survey_time(struct ath11k *ar, struct survey_info *survey,
				 u32 cc, u32 rcc, u32 cc_prev, u32 rcc_prev)
{
	u32 cc_fix = 0;
	u32 rcc_fix = 0;

	survey->filled |= SURVEY_INFO_TIME |
			  SURVEY_INFO_TIME_BUSY;

	/* TODO: Confirm if wraparound is needed */
	if (cc < cc_prev || rcc < rcc_prev) {
		if (cc < cc_prev)
			cc_fix = 0x7fffffff;

		if (rcc < rcc_prev)
			rcc_fix = 0x7fffffff;
	}

	cc -= cc_prev - cc_fix;
	rcc -= rcc_prev - rcc_fix;

	/* TODO: Divide by channel counters value */
	survey->time =  cc;
	survey->time_busy = rcc;
}

static const struct ieee80211_ops ath11k_ops = {
	.tx				= ath11k_mac_op_tx,
	.start                          = ath11k_start,
	.stop                           = ath11k_stop,
	.add_interface                  = ath11k_add_interface,
	.remove_interface		= ath11k_remove_interface,
	.config                         = ath11k_config,
	.bss_info_changed               = ath11k_bss_info_changed,
	.configure_filter		= ath11k_configure_filter,
	.hw_scan                        = ath11k_hw_scan,
	.cancel_hw_scan                 = ath11k_cancel_hw_scan,
	.set_key                        = ath11k_set_key,
	.set_default_unicast_key        = ath11k_set_default_unicast_key,
	.sta_state                      = ath11k_sta_state,
	.sta_rc_update			= ath11k_sta_rc_update,
	.conf_tx                        = ath11k_conf_tx,
	.set_antenna			= ath11k_set_antenna,
	.get_antenna			= ath11k_get_antenna,
	.ampdu_action			= ath11k_ampdu_action,
	.add_chanctx			= ath11k_mac_op_add_chanctx,
	.remove_chanctx			= ath11k_mac_op_remove_chanctx,
	.change_chanctx			= ath11k_mac_op_change_chanctx,
	.assign_vif_chanctx		= ath11k_mac_op_assign_vif_chanctx,
	.unassign_vif_chanctx		= ath11k_mac_op_unassign_vif_chanctx,
	.switch_vif_chanctx		= ath11k_mac_op_switch_vif_chanctx,
	.set_rts_threshold		= ath11k_set_rts_threshold,
	.set_frag_threshold		= ath11k_set_frag_threshold,
	.set_bitrate_mask		= ath11k_mac_op_set_bitrate_mask,
	.get_survey			= ath11k_get_survey,
	.flush				= ath11k_flush,
	CFG80211_TESTMODE_CMD(ath11k_tm_cmd)
#ifdef CONFIG_MAC80211_DEBUGFS
	.sta_add_debugfs		= ath11k_sta_add_debugfs,
#endif
};

static const struct ieee80211_iface_limit ath11k_if_limits[] = {
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_STATION),
	},
	{
		.max    = 16,
		.types  = BIT(NL80211_IFTYPE_AP)
	},
};

static const struct ieee80211_iface_combination ath11k_if_comb[] = {
	{
		.limits = ath11k_if_limits,
		.n_limits = ARRAY_SIZE(ath11k_if_limits),
		.max_interfaces = 16,
		.num_different_channels = 1,
		.beacon_int_infra_match = true,
	},
};

static int ath11k_mac_setup_channels_rates(struct ath11k *ar,
					   u32 supported_bands)
{
	struct ieee80211_supported_band *band;
	void *channels;

	BUILD_BUG_ON((ARRAY_SIZE(ath11k_2ghz_channels) +
		      ARRAY_SIZE(ath11k_5ghz_channels)) !=
		     ATH11K_NUM_CHANS);

	if (supported_bands & WMI_HOST_WLAN_2G_CAP) {
		channels = kmemdup(ath11k_2ghz_channels,
				   sizeof(ath11k_2ghz_channels),
				   GFP_KERNEL);
		if (!channels)
			return -ENOMEM;

		band = &ar->mac.sbands[NL80211_BAND_2GHZ];
		band->n_channels = ARRAY_SIZE(ath11k_2ghz_channels);
		band->channels = channels;
		band->n_bitrates = ath11k_g_rates_size;
		band->bitrates = ath11k_g_rates;
		ar->hw->wiphy->bands[NL80211_BAND_2GHZ] = band;
	}

	if (supported_bands & WMI_HOST_WLAN_5G_CAP) {
		channels = kmemdup(ath11k_5ghz_channels,
				   sizeof(ath11k_5ghz_channels),
				   GFP_KERNEL);
		if (!channels) {
			kfree(ar->mac.sbands[NL80211_BAND_2GHZ].channels);
			return -ENOMEM;
		}

		band = &ar->mac.sbands[NL80211_BAND_5GHZ];
		band->n_channels = ARRAY_SIZE(ath11k_5ghz_channels);
		band->channels = channels;
		band->n_bitrates = ath11k_a_rates_size;
		band->bitrates = ath11k_a_rates;
		ar->hw->wiphy->bands[NL80211_BAND_5GHZ] = band;
	}

	return 0;
}

static int ath11k_mac_register(struct ath11k *ar)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_pdev_cap *cap = &ar->pdev->cap;
	static const u32 cipher_suites[] = {
		WLAN_CIPHER_SUITE_WEP40,
		WLAN_CIPHER_SUITE_WEP104,
		WLAN_CIPHER_SUITE_TKIP,
		WLAN_CIPHER_SUITE_CCMP,
		WLAN_CIPHER_SUITE_AES_CMAC,
	};
	int ret;
	u32 ht_cap = 0;

	ath11k_pdev_caps_update(ar);

	SET_IEEE80211_PERM_ADDR(ar->hw, ar->mac_addr);

	SET_IEEE80211_DEV(ar->hw, ab->dev);

	ret = ath11k_mac_setup_channels_rates(ar,
					      cap->supported_bands);
	if (ret)
		return ret;

	ath11k_mac_setup_ht_vht_cap(ar, cap, &ht_cap);

	ar->hw->wiphy->available_antennas_rx = cap->rx_chain_mask;
	ar->hw->wiphy->available_antennas_tx = cap->tx_chain_mask;

	ar->hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) |
					 BIT(NL80211_IFTYPE_AP);

	ieee80211_hw_set(ar->hw, SIGNAL_DBM);
	ieee80211_hw_set(ar->hw, SUPPORTS_PS);
	ieee80211_hw_set(ar->hw, SUPPORTS_DYNAMIC_PS);
	ieee80211_hw_set(ar->hw, MFP_CAPABLE);
	ieee80211_hw_set(ar->hw, REPORTS_TX_ACK_STATUS);
	ieee80211_hw_set(ar->hw, HAS_RATE_CONTROL);
	ieee80211_hw_set(ar->hw, AP_LINK_PS);
	ieee80211_hw_set(ar->hw, SPECTRUM_MGMT);
	ieee80211_hw_set(ar->hw, SUPPORT_FAST_XMIT);
	ieee80211_hw_set(ar->hw, CONNECTION_MONITOR);
	ieee80211_hw_set(ar->hw, SUPPORTS_PER_STA_GTK);
	ieee80211_hw_set(ar->hw, WANT_MONITOR_VIF);
	ieee80211_hw_set(ar->hw, CHANCTX_STA_CSA);
	ieee80211_hw_set(ar->hw, QUEUE_CONTROL);
	ieee80211_hw_set(ar->hw, SUPPORTS_TX_FRAG);
	ieee80211_hw_set(ar->hw, REPORTS_LOW_ACK);
	if (ht_cap & WMI_HT_CAP_ENABLED) {
		ieee80211_hw_set(ar->hw, AMPDU_AGGREGATION);
		ieee80211_hw_set(ar->hw, TX_AMPDU_SETUP_IN_HW);
		ieee80211_hw_set(ar->hw, SUPPORTS_REORDERING_BUFFER);
	}

	ar->hw->wiphy->features |= NL80211_FEATURE_STATIC_SMPS;
	ar->hw->wiphy->flags |= WIPHY_FLAG_IBSS_RSN;

	/* TODO: Check if HT capability advertised from firmware is different
	 * for each band for a dual band capable radio. It will be tricky to
	 * handle it when the ht capability different for each band.
	 */
	if (ht_cap & WMI_HT_CAP_DYNAMIC_SMPS)
		ar->hw->wiphy->features |= NL80211_FEATURE_DYNAMIC_SMPS;

	ar->hw->wiphy->max_scan_ssids = WLAN_SCAN_PARAMS_MAX_SSID;
	ar->hw->wiphy->max_scan_ie_len = WLAN_SCAN_PARAMS_MAX_IE_LEN;

	ar->hw->max_listen_interval = ATH11K_MAX_HW_LISTEN_INTERVAL;

	ar->hw->wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
	ar->hw->wiphy->max_remain_on_channel_duration = 5000;

	ar->hw->wiphy->flags |= WIPHY_FLAG_AP_UAPSD;
	ar->hw->wiphy->features |= NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE |
				   NL80211_FEATURE_AP_SCAN;

	ar->hw->wiphy->max_ap_assoc_sta = ar->max_num_stations;

	ar->hw->queues = ATH11K_HW_MAX_QUEUES;
	ar->hw->offchannel_tx_hw_queue = ATH11K_HW_MAX_QUEUES - 1;

	ar->hw->vif_data_size = sizeof(struct ath11k_vif);
	ar->hw->sta_data_size = sizeof(struct ath11k_sta);

	ar->hw->wiphy->iface_combinations = ath11k_if_comb;
	ar->hw->wiphy->n_iface_combinations = ARRAY_SIZE(ath11k_if_comb);

	wiphy_ext_feature_set(ar->hw->wiphy, NL80211_EXT_FEATURE_CQM_RSSI_LIST);

	ar->hw->wiphy->cipher_suites = cipher_suites;
	ar->hw->wiphy->n_cipher_suites = ARRAY_SIZE(cipher_suites);

	ath11k_reg_init(ar);

	ret = ieee80211_register_hw(ar->hw);
	if (ret) {
		ath11k_err(ar->ab, "ieee80211 registration failed: %d\n", ret);
		goto err_free;
	}

	/* Apply the regd received during initialization */
	ret = ath11k_regd_update(ar, true);
	if (ret) {
		ath11k_err(ar->ab, "ath11k regd update failed: %d\n", ret);
		goto err_free;
	}

	ret = ath11k_debug_register(ar);
	if (ret) {
		ath11k_err(ar->ab, "debugfs registration failed: %d\n", ret);
		goto err_free;
	}

	return 0;

err_free:
	kfree(ar->mac.sbands[NL80211_BAND_2GHZ].channels);
	kfree(ar->mac.sbands[NL80211_BAND_5GHZ].channels);

	SET_IEEE80211_DEV(ar->hw, NULL);
	return ret;
}

void ath11k_mac_destroy(struct ath11k_base *ab)
{
	struct ath11k *ar;
	struct ath11k_pdev *pdev;
	int i;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		if (!ar)
			continue;
		cancel_work_sync(&ar->regd_update_work);

		ieee80211_unregister_hw(ar->hw);

		idr_for_each(&ar->txmgmt_idr,
			     ath11k_mac_tx_mgmt_pending_free, ar);
		idr_destroy(&ar->txmgmt_idr);

		kfree(ar->mac.sbands[NL80211_BAND_2GHZ].channels);
		kfree(ar->mac.sbands[NL80211_BAND_5GHZ].channels);

		kfree(ab->default_regd[i]);
		kfree(ab->new_regd[i]);

		SET_IEEE80211_DEV(ar->hw, NULL);

		ieee80211_free_hw(ar->hw);

		pdev->ar = NULL;
	}

	ab->mac_registered = false;
}

int ath11k_mac_create(struct ath11k_base *ab)
{
	struct ieee80211_hw *hw;
	struct ath11k *ar;
	struct ath11k_pdev *pdev;
	int ret;
	int i;

	if (ab->mac_registered)
		return 0;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		hw = ieee80211_alloc_hw(sizeof(struct ath11k), &ath11k_ops);
		if (!hw) {
			ath11k_warn(ab, "failed to allocate mac80211 hw device\n");
			ret = -ENOMEM;
			goto err_destroy_mac;
		}

		ar = hw->priv;
		ar->hw = hw;
		ar->ab = ab;
		ar->pdev = pdev;
		ar->pdev_idx = i;
		ar->lmac_id = hw_mac_id_map[i];

		ar->wmi = &ab->wmi_sc.wmi[i];
		/* FIXME wmi[0] is already initialized during attach,
		 * Should we do this again?
		 */
		ath11k_wmi_pdev_attach(ab, i);

		ar->cfg_tx_chainmask = pdev->cap.tx_chain_mask;
		ar->cfg_rx_chainmask = pdev->cap.rx_chain_mask;
		ar->num_tx_chains = get_num_chains(pdev->cap.tx_chain_mask);
		ar->num_rx_chains = get_num_chains(pdev->cap.rx_chain_mask);

		if (ab->pdevs_macaddr_valid) {
			ether_addr_copy(ar->mac_addr, pdev->mac_addr);
		} else {
			ether_addr_copy(ar->mac_addr, ab->mac_addr);
			ar->mac_addr[4] += i;
		}

		pdev->ar = ar;
		spin_lock_init(&ar->data_lock);
		INIT_LIST_HEAD(&ar->arvifs);
		mutex_init(&ar->conf_mutex);
		init_completion(&ar->vdev_setup_done);
		init_completion(&ar->peer_assoc_done);
		init_completion(&ar->install_key_done);
		init_completion(&ar->bss_survey_done);
		init_completion(&ar->scan.started);
		init_completion(&ar->scan.completed);
		INIT_DELAYED_WORK(&ar->scan.timeout, ath11k_scan_timeout_work);
		INIT_WORK(&ar->regd_update_work, ath11k_regd_update_work);

		INIT_WORK(&ar->wmi_mgmt_tx_work, ath11k_mgmt_over_wmi_tx_work);
		skb_queue_head_init(&ar->wmi_mgmt_tx_queue);

		ret = ath11k_mac_register(ar);
		if (ret) {
			ath11k_warn(ab, "failed to register hw device\n");
			pdev->ar = NULL;
			ieee80211_free_hw(hw);
			goto err_destroy_mac;
		}

		idr_init(&ar->txmgmt_idr);
		spin_lock_init(&ar->txmgmt_idr_lock);

	}

	ab->free_vdev_map = (1LL << (ab->num_radios * TARGET_NUM_VDEVS)) - 1;

	ab->mac_registered = true;

	return 0;

err_destroy_mac:
	ath11k_mac_destroy(ab);

	return ret;
}
