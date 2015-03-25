/*
 * Copyright 2006 Dave Airlie <airlied@linux.ie>
 * Copyright © 2006-2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Eric Anholt <eric@anholt.net>
 *	Jesse Barnes <jesse.barnes@intel.com>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm.h>
#include <dev/drm2/drm_crtc.h>
#include <dev/drm2/drm_edid.h>
#include <dev/drm2/i915/i915_drm.h>
#include <dev/drm2/i915/i915_drv.h>
#include <dev/drm2/i915/intel_drv.h>

struct intel_hdmi *enc_to_intel_hdmi(struct drm_encoder *encoder)
{
	return container_of(encoder, struct intel_hdmi, base.base);
}

static struct intel_hdmi *intel_attached_hdmi(struct drm_connector *connector)
{
	return container_of(intel_attached_encoder(connector),
			    struct intel_hdmi, base);
}

void intel_dip_infoframe_csum(struct dip_infoframe *frame)
{
	uint8_t *data = (uint8_t *)frame;
	uint8_t sum = 0;
	unsigned i;

	frame->checksum = 0;
	frame->ecc = 0;

	for (i = 0; i < frame->len + DIP_HEADER_SIZE; i++)
		sum += data[i];

	frame->checksum = 0x100 - sum;
}

static u32 g4x_infoframe_index(struct dip_infoframe *frame)
{
	switch (frame->type) {
	case DIP_TYPE_AVI:
		return VIDEO_DIP_SELECT_AVI;
	case DIP_TYPE_SPD:
		return VIDEO_DIP_SELECT_SPD;
	default:
		DRM_DEBUG_DRIVER("unknown info frame type %d\n", frame->type);
		return 0;
	}
}

static u32 g4x_infoframe_enable(struct dip_infoframe *frame)
{
	switch (frame->type) {
	case DIP_TYPE_AVI:
		return VIDEO_DIP_ENABLE_AVI;
	case DIP_TYPE_SPD:
		return VIDEO_DIP_ENABLE_SPD;
	default:
		DRM_DEBUG_DRIVER("unknown info frame type %d\n", frame->type);
		return 0;
	}
}

static u32 hsw_infoframe_enable(struct dip_infoframe *frame)
{
	switch (frame->type) {
	case DIP_TYPE_AVI:
		return VIDEO_DIP_ENABLE_AVI_HSW;
	case DIP_TYPE_SPD:
		return VIDEO_DIP_ENABLE_SPD_HSW;
	default:
		DRM_DEBUG_DRIVER("unknown info frame type %d\n", frame->type);
		return 0;
	}
}

static u32 hsw_infoframe_data_reg(struct dip_infoframe *frame, enum pipe pipe)
{
	switch (frame->type) {
	case DIP_TYPE_AVI:
		return HSW_TVIDEO_DIP_AVI_DATA(pipe);
	case DIP_TYPE_SPD:
		return HSW_TVIDEO_DIP_SPD_DATA(pipe);
	default:
		DRM_DEBUG_DRIVER("unknown info frame type %d\n", frame->type);
		return 0;
	}
}

static void g4x_write_infoframe(struct drm_encoder *encoder,
				struct dip_infoframe *frame)
{
	uint32_t *data = (uint32_t *)frame;
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_hdmi *intel_hdmi = enc_to_intel_hdmi(encoder);
	u32 val = I915_READ(VIDEO_DIP_CTL);
	unsigned i, len = DIP_HEADER_SIZE + frame->len;

	val &= ~VIDEO_DIP_PORT_MASK;
	if (intel_hdmi->sdvox_reg == SDVOB)
		val |= VIDEO_DIP_PORT_B;
	else if (intel_hdmi->sdvox_reg == SDVOC)
		val |= VIDEO_DIP_PORT_C;
	else
		return;

	val &= ~(VIDEO_DIP_SELECT_MASK | 0xf); /* clear DIP data offset */
	val |= g4x_infoframe_index(frame);

	val &= ~g4x_infoframe_enable(frame);
	val |= VIDEO_DIP_ENABLE;

	I915_WRITE(VIDEO_DIP_CTL, val);

	for (i = 0; i < len; i += 4) {
		I915_WRITE(VIDEO_DIP_DATA, *data);
		data++;
	}

	val |= g4x_infoframe_enable(frame);
	val &= ~VIDEO_DIP_FREQ_MASK;
	val |= VIDEO_DIP_FREQ_VSYNC;

	I915_WRITE(VIDEO_DIP_CTL, val);
}

static void ibx_write_infoframe(struct drm_encoder *encoder,
				struct dip_infoframe *frame)
{
	uint32_t *data = (uint32_t *)frame;
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->crtc);
	struct intel_hdmi *intel_hdmi = enc_to_intel_hdmi(encoder);
	int reg = TVIDEO_DIP_CTL(intel_crtc->pipe);
	unsigned i, len = DIP_HEADER_SIZE + frame->len;
	u32 val = I915_READ(reg);

	val &= ~VIDEO_DIP_PORT_MASK;
	switch (intel_hdmi->sdvox_reg) {
	case HDMIB:
		val |= VIDEO_DIP_PORT_B;
		break;
	case HDMIC:
		val |= VIDEO_DIP_PORT_C;
		break;
	case HDMID:
		val |= VIDEO_DIP_PORT_D;
		break;
	default:
		return;
	}

	intel_wait_for_vblank(dev, intel_crtc->pipe);

	val &= ~(VIDEO_DIP_SELECT_MASK | 0xf); /* clear DIP data offset */
	val |= g4x_infoframe_index(frame);

	val &= ~g4x_infoframe_enable(frame);
	val |= VIDEO_DIP_ENABLE;

	I915_WRITE(reg, val);

	for (i = 0; i < len; i += 4) {
		I915_WRITE(TVIDEO_DIP_DATA(intel_crtc->pipe), *data);
		data++;
	}

	val |= g4x_infoframe_enable(frame);
	val &= ~VIDEO_DIP_FREQ_MASK;
	val |= VIDEO_DIP_FREQ_VSYNC;

	I915_WRITE(reg, val);
}

static void cpt_write_infoframe(struct drm_encoder *encoder,
				struct dip_infoframe *frame)
{
	uint32_t *data = (uint32_t *)frame;
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->crtc);
	int reg = TVIDEO_DIP_CTL(intel_crtc->pipe);
	unsigned i, len = DIP_HEADER_SIZE + frame->len;
	u32 val = I915_READ(reg);

	intel_wait_for_vblank(dev, intel_crtc->pipe);

	val &= ~(VIDEO_DIP_SELECT_MASK | 0xf); /* clear DIP data offset */
	val |= g4x_infoframe_index(frame);

	/* The DIP control register spec says that we need to update the AVI
	 * infoframe without clearing its enable bit */
	if (frame->type == DIP_TYPE_AVI)
		val |= VIDEO_DIP_ENABLE_AVI;
	else
		val &= ~g4x_infoframe_enable(frame);

	val |= VIDEO_DIP_ENABLE;

	I915_WRITE(reg, val);

	for (i = 0; i < len; i += 4) {
		I915_WRITE(TVIDEO_DIP_DATA(intel_crtc->pipe), *data);
		data++;
	}

	val |= g4x_infoframe_enable(frame);
	val &= ~VIDEO_DIP_FREQ_MASK;
	val |= VIDEO_DIP_FREQ_VSYNC;

	I915_WRITE(reg, val);
}

static void vlv_write_infoframe(struct drm_encoder *encoder,
				     struct dip_infoframe *frame)
{
	uint32_t *data = (uint32_t *)frame;
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->crtc);
	int reg = VLV_TVIDEO_DIP_CTL(intel_crtc->pipe);
	unsigned i, len = DIP_HEADER_SIZE + frame->len;
	u32 val = I915_READ(reg);

	intel_wait_for_vblank(dev, intel_crtc->pipe);

	val &= ~(VIDEO_DIP_SELECT_MASK | 0xf); /* clear DIP data offset */
	val |= g4x_infoframe_index(frame);

	val &= ~g4x_infoframe_enable(frame);
	val |= VIDEO_DIP_ENABLE;

	I915_WRITE(reg, val);

	for (i = 0; i < len; i += 4) {
		I915_WRITE(VLV_TVIDEO_DIP_DATA(intel_crtc->pipe), *data);
		data++;
	}

	val |= g4x_infoframe_enable(frame);
	val &= ~VIDEO_DIP_FREQ_MASK;
	val |= VIDEO_DIP_FREQ_VSYNC;

	I915_WRITE(reg, val);
}

static void hsw_write_infoframe(struct drm_encoder *encoder,
				struct dip_infoframe *frame)
{
	uint32_t *data = (uint32_t *)frame;
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->crtc);
	u32 ctl_reg = HSW_TVIDEO_DIP_CTL(intel_crtc->pipe);
	u32 data_reg = hsw_infoframe_data_reg(frame, intel_crtc->pipe);
	unsigned int i, len = DIP_HEADER_SIZE + frame->len;
	u32 val = I915_READ(ctl_reg);

	if (data_reg == 0)
		return;

	intel_wait_for_vblank(dev, intel_crtc->pipe);

	val &= ~hsw_infoframe_enable(frame);
	I915_WRITE(ctl_reg, val);

	for (i = 0; i < len; i += 4) {
		I915_WRITE(data_reg + i, *data);
		data++;
	}

	val |= hsw_infoframe_enable(frame);
	I915_WRITE(ctl_reg, val);
}

static void intel_set_infoframe(struct drm_encoder *encoder,
				struct dip_infoframe *frame)
{
	struct intel_hdmi *intel_hdmi = enc_to_intel_hdmi(encoder);

	if (!intel_hdmi->has_hdmi_sink)
		return;

	intel_dip_infoframe_csum(frame);
	intel_hdmi->write_infoframe(encoder, frame);
}

void intel_hdmi_set_avi_infoframe(struct drm_encoder *encoder,
					 struct drm_display_mode *adjusted_mode)
{
	struct dip_infoframe avi_if = {
		.type = DIP_TYPE_AVI,
		.ver = DIP_VERSION_AVI,
		.len = DIP_LEN_AVI,
	};

	if (adjusted_mode->flags & DRM_MODE_FLAG_DBLCLK)
		avi_if.body.avi.YQ_CN_PR |= DIP_AVI_PR_2;

	intel_set_infoframe(encoder, &avi_if);
}

void intel_hdmi_set_spd_infoframe(struct drm_encoder *encoder)
{
	struct dip_infoframe spd_if;

	memset(&spd_if, 0, sizeof(spd_if));
	spd_if.type = DIP_TYPE_SPD;
	spd_if.ver = DIP_VERSION_SPD;
	spd_if.len = DIP_LEN_SPD;
	strcpy(spd_if.body.spd.vn, "Intel");
	strcpy(spd_if.body.spd.pd, "Integrated gfx");
	spd_if.body.spd.sdi = DIP_SPD_PC;

	intel_set_infoframe(encoder, &spd_if);
}

static void intel_hdmi_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->crtc);
	struct intel_hdmi *intel_hdmi = enc_to_intel_hdmi(encoder);
	u32 sdvox;

	sdvox = SDVO_ENCODING_HDMI | SDVO_BORDER_ENABLE;
	if (!HAS_PCH_SPLIT(dev))
		sdvox |= intel_hdmi->color_range;
	if (adjusted_mode->flags & DRM_MODE_FLAG_PVSYNC)
		sdvox |= SDVO_VSYNC_ACTIVE_HIGH;
	if (adjusted_mode->flags & DRM_MODE_FLAG_PHSYNC)
		sdvox |= SDVO_HSYNC_ACTIVE_HIGH;

	if (intel_crtc->bpp > 24)
		sdvox |= COLOR_FORMAT_12bpc;
	else
		sdvox |= COLOR_FORMAT_8bpc;

	/* Required on CPT */
	if (intel_hdmi->has_hdmi_sink && HAS_PCH_CPT(dev))
		sdvox |= HDMI_MODE_SELECT;

	if (intel_hdmi->has_audio) {
		DRM_DEBUG_KMS("Enabling HDMI audio on pipe %c\n",
				 pipe_name(intel_crtc->pipe));
		sdvox |= SDVO_AUDIO_ENABLE;
		sdvox |= SDVO_NULL_PACKETS_DURING_VSYNC;
		intel_write_eld(encoder, adjusted_mode);
	}

	if (HAS_PCH_CPT(dev))
		sdvox |= PORT_TRANS_SEL_CPT(intel_crtc->pipe);
	else if (intel_crtc->pipe == 1)
		sdvox |= SDVO_PIPE_B_SELECT;

	I915_WRITE(intel_hdmi->sdvox_reg, sdvox);
	POSTING_READ(intel_hdmi->sdvox_reg);

	intel_hdmi_set_avi_infoframe(encoder, adjusted_mode);
	intel_hdmi_set_spd_infoframe(encoder);
}

static void intel_hdmi_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_hdmi *intel_hdmi = enc_to_intel_hdmi(encoder);
	u32 temp;
	u32 enable_bits = SDVO_ENABLE;

	if (intel_hdmi->has_audio)
		enable_bits |= SDVO_AUDIO_ENABLE;

	temp = I915_READ(intel_hdmi->sdvox_reg);

	/* HW workaround, need to toggle enable bit off and on for 12bpc, but
	 * we do this anyway which shows more stable in testing.
	 */
	if (HAS_PCH_SPLIT(dev)) {
		I915_WRITE(intel_hdmi->sdvox_reg, temp & ~SDVO_ENABLE);
		POSTING_READ(intel_hdmi->sdvox_reg);
	}

	if (mode != DRM_MODE_DPMS_ON) {
		temp &= ~enable_bits;
	} else {
		temp |= enable_bits;
	}

	I915_WRITE(intel_hdmi->sdvox_reg, temp);
	POSTING_READ(intel_hdmi->sdvox_reg);

	/* HW workaround, need to write this twice for issue that may result
	 * in first write getting masked.
	 */
	if (HAS_PCH_SPLIT(dev)) {
		I915_WRITE(intel_hdmi->sdvox_reg, temp);
		POSTING_READ(intel_hdmi->sdvox_reg);
	}
}

static int intel_hdmi_mode_valid(struct drm_connector *connector,
				 struct drm_display_mode *mode)
{
	if (mode->clock > 165000)
		return MODE_CLOCK_HIGH;
	if (mode->clock < 20000)
		return MODE_CLOCK_LOW;

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		return MODE_NO_DBLESCAN;

	return MODE_OK;
}

static bool intel_hdmi_mode_fixup(struct drm_encoder *encoder,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	return true;
}

static enum drm_connector_status
intel_hdmi_detect(struct drm_connector *connector, bool force)
{
	struct intel_hdmi *intel_hdmi = intel_attached_hdmi(connector);
	struct drm_i915_private *dev_priv = connector->dev->dev_private;
	struct edid *edid;
	enum drm_connector_status status = connector_status_disconnected;

	intel_hdmi->has_hdmi_sink = false;
	intel_hdmi->has_audio = false;
	edid = drm_get_edid(connector, intel_gmbus_get_adapter(dev_priv,
	    intel_hdmi->ddc_bus));

	if (edid) {
		if (edid->input & DRM_EDID_INPUT_DIGITAL) {
			status = connector_status_connected;
			if (intel_hdmi->force_audio != HDMI_AUDIO_OFF_DVI)
				intel_hdmi->has_hdmi_sink =
						drm_detect_hdmi_monitor(edid);
			intel_hdmi->has_audio = drm_detect_monitor_audio(edid);
		}
		free(edid, DRM_MEM_KMS);
	} else {
		DRM_DEBUG_KMS("[CONNECTOR:%d:%s] got no edid, ddc port %d\n",
		    connector->base.id, drm_get_connector_name(connector),
		    intel_hdmi->ddc_bus);
	}

	if (status == connector_status_connected) {
		if (intel_hdmi->force_audio != HDMI_AUDIO_AUTO)
			intel_hdmi->has_audio =
				(intel_hdmi->force_audio == HDMI_AUDIO_ON);
	}

	return status;
}

static int intel_hdmi_get_modes(struct drm_connector *connector)
{
	struct intel_hdmi *intel_hdmi = intel_attached_hdmi(connector);
	struct drm_i915_private *dev_priv = connector->dev->dev_private;

	/* We should parse the EDID data and find out if it's an HDMI sink so
	 * we can send audio to it.
	 */

	return intel_ddc_get_modes(connector,
				   intel_gmbus_get_adapter(dev_priv,
							   intel_hdmi->ddc_bus));
}

static bool
intel_hdmi_detect_audio(struct drm_connector *connector)
{
	struct intel_hdmi *intel_hdmi = intel_attached_hdmi(connector);
	struct drm_i915_private *dev_priv = connector->dev->dev_private;
	struct edid *edid;
	bool has_audio = false;

	edid = drm_get_edid(connector,
			    intel_gmbus_get_adapter(dev_priv,
						    intel_hdmi->ddc_bus));
	if (edid) {
		if (edid->input & DRM_EDID_INPUT_DIGITAL)
			has_audio = drm_detect_monitor_audio(edid);

		free(edid, DRM_MEM_KMS);
	}

	return has_audio;
}

static int
intel_hdmi_set_property(struct drm_connector *connector,
		      struct drm_property *property,
		      uint64_t val)
{
	struct intel_hdmi *intel_hdmi = intel_attached_hdmi(connector);
	struct drm_i915_private *dev_priv = connector->dev->dev_private;
	int ret;

	ret = drm_object_property_set_value(&connector->base, property, val);
	if (ret)
		return ret;

	if (property == dev_priv->force_audio_property) {
		enum hdmi_force_audio i = val;
		bool has_audio;

		if (i == intel_hdmi->force_audio)
			return 0;

		intel_hdmi->force_audio = i;

		if (i == HDMI_AUDIO_AUTO)
			has_audio = intel_hdmi_detect_audio(connector);
		else
			has_audio = (i == HDMI_AUDIO_ON);

		if (i == HDMI_AUDIO_OFF_DVI)
			intel_hdmi->has_hdmi_sink = 0;

		intel_hdmi->has_audio = has_audio;
		goto done;
	}

	if (property == dev_priv->broadcast_rgb_property) {
		if (val == !!intel_hdmi->color_range)
			return 0;

		intel_hdmi->color_range = val ? SDVO_COLOR_RANGE_16_235 : 0;
		goto done;
	}

	return -EINVAL;

done:
	if (intel_hdmi->base.base.crtc) {
		struct drm_crtc *crtc = intel_hdmi->base.base.crtc;
		drm_crtc_helper_set_mode(crtc, &crtc->mode,
					 crtc->x, crtc->y,
					 crtc->fb);
	}

	return 0;
}

static void intel_hdmi_destroy(struct drm_connector *connector)
{
#if 0
	drm_sysfs_connector_remove(connector);
#endif
	drm_connector_cleanup(connector);
	free(connector, DRM_MEM_KMS);
}

static const struct drm_encoder_helper_funcs intel_hdmi_helper_funcs_hsw = {
	.dpms = intel_ddi_dpms,
	.mode_fixup = intel_hdmi_mode_fixup,
	.prepare = intel_encoder_prepare,
	.mode_set = intel_ddi_mode_set,
	.commit = intel_encoder_commit,
};

static const struct drm_encoder_helper_funcs intel_hdmi_helper_funcs = {
	.dpms = intel_hdmi_dpms,
	.mode_fixup = intel_hdmi_mode_fixup,
	.prepare = intel_encoder_prepare,
	.mode_set = intel_hdmi_mode_set,
	.commit = intel_encoder_commit,
};

static const struct drm_connector_funcs intel_hdmi_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = intel_hdmi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.set_property = intel_hdmi_set_property,
	.destroy = intel_hdmi_destroy,
};

static const struct drm_connector_helper_funcs intel_hdmi_connector_helper_funcs = {
	.get_modes = intel_hdmi_get_modes,
	.mode_valid = intel_hdmi_mode_valid,
	.best_encoder = intel_best_encoder,
};

static const struct drm_encoder_funcs intel_hdmi_enc_funcs = {
	.destroy = intel_encoder_destroy,
};

static void
intel_hdmi_add_properties(struct intel_hdmi *intel_hdmi, struct drm_connector *connector)
{
	intel_attach_force_audio_property(connector);
	intel_attach_broadcast_rgb_property(connector);
}

void intel_hdmi_init(struct drm_device *dev, int sdvox_reg)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_connector *connector;
	struct intel_encoder *intel_encoder;
	struct intel_connector *intel_connector;
	struct intel_hdmi *intel_hdmi;
	int i;

	intel_hdmi = malloc(sizeof(struct intel_hdmi), DRM_MEM_KMS,
	    M_WAITOK | M_ZERO);
	intel_connector = malloc(sizeof(struct intel_connector), DRM_MEM_KMS,
	    M_WAITOK | M_ZERO);

	intel_encoder = &intel_hdmi->base;
	drm_encoder_init(dev, &intel_encoder->base, &intel_hdmi_enc_funcs,
			 DRM_MODE_ENCODER_TMDS);

	connector = &intel_connector->base;
	drm_connector_init(dev, connector, &intel_hdmi_connector_funcs,
			   DRM_MODE_CONNECTOR_HDMIA);
	drm_connector_helper_add(connector, &intel_hdmi_connector_helper_funcs);

	intel_encoder->type = INTEL_OUTPUT_HDMI;

	connector->polled = DRM_CONNECTOR_POLL_HPD;
	connector->interlace_allowed = 1;
	connector->doublescan_allowed = 0;
	intel_encoder->crtc_mask = (1 << 0) | (1 << 1) | (1 << 2);

	/* Set up the DDC bus. */
	if (sdvox_reg == SDVOB) {
		intel_encoder->clone_mask = (1 << INTEL_HDMIB_CLONE_BIT);
		intel_hdmi->ddc_bus = GMBUS_PORT_DPB;
		dev_priv->hotplug_supported_mask |= HDMIB_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == SDVOC) {
		intel_encoder->clone_mask = (1 << INTEL_HDMIC_CLONE_BIT);
		intel_hdmi->ddc_bus = GMBUS_PORT_DPC;
		dev_priv->hotplug_supported_mask |= HDMIC_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == HDMIB) {
		intel_encoder->clone_mask = (1 << INTEL_HDMID_CLONE_BIT);
		intel_hdmi->ddc_bus = GMBUS_PORT_DPB;
		dev_priv->hotplug_supported_mask |= HDMIB_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == HDMIC) {
		intel_encoder->clone_mask = (1 << INTEL_HDMIE_CLONE_BIT);
		intel_hdmi->ddc_bus = GMBUS_PORT_DPC;
		dev_priv->hotplug_supported_mask |= HDMIC_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == HDMID) {
		intel_encoder->clone_mask = (1 << INTEL_HDMIF_CLONE_BIT);
		intel_hdmi->ddc_bus = GMBUS_PORT_DPD;
		dev_priv->hotplug_supported_mask |= HDMID_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == DDI_BUF_CTL(PORT_B)) {
		DRM_DEBUG_DRIVER("LPT: detected output on DDI B\n");
		intel_encoder->clone_mask = (1 << INTEL_HDMIB_CLONE_BIT);
		intel_hdmi->ddc_bus = GMBUS_PORT_DPB;
		intel_hdmi->ddi_port = PORT_B;
		dev_priv->hotplug_supported_mask |= HDMIB_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == DDI_BUF_CTL(PORT_C)) {
		DRM_DEBUG_DRIVER("LPT: detected output on DDI C\n");
		intel_encoder->clone_mask = (1 << INTEL_HDMIC_CLONE_BIT);
		intel_hdmi->ddc_bus = GMBUS_PORT_DPC;
		intel_hdmi->ddi_port = PORT_C;
		dev_priv->hotplug_supported_mask |= HDMIC_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == DDI_BUF_CTL(PORT_D)) {
		DRM_DEBUG_DRIVER("LPT: detected output on DDI D\n");
		intel_encoder->clone_mask = (1 << INTEL_HDMID_CLONE_BIT);
		intel_hdmi->ddc_bus = GMBUS_PORT_DPD;
		intel_hdmi->ddi_port = PORT_D;
		dev_priv->hotplug_supported_mask |= HDMID_HOTPLUG_INT_STATUS;
	} else {
		/* If we got an unknown sdvox_reg, things are pretty much broken
		 * in a way that we should let the kernel know about it */
		DRM_DEBUG_KMS("unknown sdvox_reg %d\n", sdvox_reg);
	}

	intel_hdmi->sdvox_reg = sdvox_reg;
	if (!HAS_PCH_SPLIT(dev)) {
		intel_hdmi->write_infoframe = g4x_write_infoframe;
		I915_WRITE(VIDEO_DIP_CTL, 0);
	} else if (IS_VALLEYVIEW(dev)) {
		intel_hdmi->write_infoframe = vlv_write_infoframe;
		for_each_pipe(i)
			I915_WRITE(VLV_TVIDEO_DIP_CTL(i), 0);
	} else if (IS_HASWELL(dev)) {
		/* FIXME: Haswell has a new set of DIP frame registers, but we are
		 * just doing the minimal required for HDMI to work at this stage.
		 */
		intel_hdmi->write_infoframe = hsw_write_infoframe;
		for_each_pipe(i)
			I915_WRITE(HSW_TVIDEO_DIP_CTL(i), 0);
	} else if (HAS_PCH_IBX(dev)) {
		intel_hdmi->write_infoframe = ibx_write_infoframe;
		for_each_pipe(i)
			I915_WRITE(TVIDEO_DIP_CTL(i), 0);
	} else {
		intel_hdmi->write_infoframe = cpt_write_infoframe;
		for_each_pipe(i)
			I915_WRITE(TVIDEO_DIP_CTL(i), 0);
	}

	if (IS_HASWELL(dev))
		drm_encoder_helper_add(&intel_encoder->base, &intel_hdmi_helper_funcs_hsw);
	else
		drm_encoder_helper_add(&intel_encoder->base, &intel_hdmi_helper_funcs);

	intel_hdmi_add_properties(intel_hdmi, connector);

	intel_connector_attach_encoder(intel_connector, intel_encoder);
#if 0
	drm_sysfs_connector_add(connector);
#endif

	/* For G4X desktop chip, PEG_BAND_GAP_DATA 3:0 must first be written
	 * 0xd.  Failure to do so will result in spurious interrupts being
	 * generated on the port when a cable is not attached.
	 */
	if (IS_G4X(dev) && !IS_GM45(dev)) {
		u32 temp = I915_READ(PEG_BAND_GAP_DATA);
		I915_WRITE(PEG_BAND_GAP_DATA, (temp & ~0xf) | 0xd);
	}
}
