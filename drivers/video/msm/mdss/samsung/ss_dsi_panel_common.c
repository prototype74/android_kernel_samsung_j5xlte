/*
 * =================================================================
 *
 *
 *	Description:  samsung display common file
 *
 *	Author: jb09.kim
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2012, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/
#include "ss_dsi_panel_common.h"
#include "../mdss_debug.h"

static void mdss_samsung_event_osc_te_fitting(struct mdss_panel_data *pdata, int event, void *arg);
static irqreturn_t samsung_te_check_handler(int irq, void *handle);
static void samsung_te_check_done_work(struct work_struct *work);
static void mdss_samsung_event_esd_recovery_init(struct mdss_panel_data *pdata, int event, void *arg);
struct samsung_display_driver_data vdd_data;

struct dsi_cmd_desc default_cmd = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};

#if defined(CONFIG_LCD_CLASS_DEVICE)
/* SYSFS RELATED VALUE */
#define MAX_FILE_NAME 128
#define TUNING_FILE_PATH "/sdcard/"
static char tuning_file[MAX_FILE_NAME];
#endif
u8 csc_update = 1;
u8 csc_change = 0;

void __iomem *virt_mmss_gp_base;

struct samsung_display_driver_data * check_valid_ctrl(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = NULL;

	if (IS_ERR_OR_NULL(ctrl)) {
		LCD_ERR("Invalid data ctrl : 0x%zx\n", (size_t)ctrl);
		return NULL;
	}

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid vdd_data : 0x%zx\n", (size_t)vdd);
		return NULL;
	}

	return vdd;
}

char mdss_panel_id0_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return (vdd->manufacture_id_dsi[display_ndx_check(ctrl)] & 0xFF0000) >> 16;
}

char mdss_panel_id1_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return (vdd->manufacture_id_dsi[display_ndx_check(ctrl)] & 0xFF00) >> 8;
}

char mdss_panel_id2_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return vdd->manufacture_id_dsi[display_ndx_check(ctrl)] & 0xFF;
}

int mdss_panel_attach_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return (vdd->panel_attach_status & (0x01 << ctrl->ndx)) > 0 ? true : false;
}

int mdss_panel_attach_set(struct mdss_dsi_ctrl_pdata *ctrl, int status)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	/* 0bit->DSI0 1bit->DSI1 */
	if (likely(get_lcd_attached("GET") || get_lcd_attached_secondary("GET")) && status) {
		if (ctrl->cmd_sync_wait_broadcast)
			vdd->panel_attach_status |= (BIT(1) | BIT(0));
		else {
			/* One more time check dual dsi */
			if (!IS_ERR_OR_NULL(vdd->ctrl_dsi[DSI_CTRL_0]) &&
				!IS_ERR_OR_NULL(vdd->ctrl_dsi[DSI_CTRL_1]))
				vdd->panel_attach_status |= (BIT(1) | BIT(0));
			else
				vdd->panel_attach_status |= BIT(0) << ctrl->ndx;
		}
	} else {
		if (ctrl->cmd_sync_wait_broadcast)
			vdd->panel_attach_status &= ~(BIT(1) | BIT(0));
		else {
			/* One more time check dual dsi */
			if (!IS_ERR_OR_NULL(vdd->ctrl_dsi[DSI_CTRL_0]) &&
				!IS_ERR_OR_NULL(vdd->ctrl_dsi[DSI_CTRL_1]))
				vdd->panel_attach_status &= ~(BIT(1) | BIT(0));
			else
				vdd->panel_attach_status &= ~(BIT(0) << ctrl->ndx);
		}
	}

	LCD_INFO("panel_attach_status : %d\n", vdd->panel_attach_status);

	return vdd->panel_attach_status;
}

int get_lcd_attached(char *mode)
{
	char *pt;
	static int lcd_id = 0;

	LCD_DEBUG("%s", mode);

	if (mode == NULL)
		return true;

	if (!strncmp(mode, "GET", 3)) {
		return lcd_id;
	} else {
		lcd_id = 0;
		for (pt = mode; *pt != 0; pt++)  {
			lcd_id <<= 4;
			switch (*pt) {
			case '0' ... '9':
				lcd_id += *pt - '0';
				break;
			case 'a' ... 'f':
				lcd_id += 10 + *pt - 'a';
				break;
			case 'A' ... 'F':
				lcd_id += 10 + *pt - 'A';
				break;
			}
		}
		LCD_ERR("LCD_ID = 0x%X\n", lcd_id);
	}

	return lcd_id;
}
EXPORT_SYMBOL(get_lcd_attached);
__setup("lcd_id=0x", get_lcd_attached);

int get_lcd_attached_secondary(char *mode)
{
	char *pt;
	static int lcd_id = 0;

	LCD_DEBUG("%s", mode);

	if (mode == NULL)
		return true;

	if (!strncmp(mode, "GET", 3)) {
		return lcd_id;
	} else {
		for (pt = mode; *pt != 0; pt++)  {
			lcd_id <<= 4;
			switch (*pt) {
			case '0' ... '9':
				lcd_id += *pt - '0';
				break;
			case 'a' ... 'f':
				lcd_id += 10 + *pt - 'a';
				break;
			case 'A' ... 'F':
				lcd_id += 10 + *pt - 'A';
				break;
			}
		}
		LCD_ERR("LCD_ID2 = 0x%X\n", lcd_id);
	}

	return lcd_id;
}
EXPORT_SYMBOL(get_lcd_attached_secondary);
__setup("lcd_id2=0x", get_lcd_attached_secondary);

int get_hall_ic_status(char *mode)
{
	if (mode == NULL)
		return true;

	if (*mode - '0')
		vdd_data.display_ststus_dsi[DISPLAY_1].hall_ic_status = HALL_IC_CLOSE;
	else
		vdd_data.display_ststus_dsi[DISPLAY_1].hall_ic_status = HALL_IC_OPEN;

	LCD_ERR("hall_ic : %s\n", vdd_data.display_ststus_dsi[DISPLAY_1].hall_ic_status ? "CLOSE" : "OPEN");

	return true;
}
EXPORT_SYMBOL(get_hall_ic_status);
__setup("hall_ic=0x", get_hall_ic_status);

static void mdss_samsung_event_frame_update(struct mdss_panel_data *pdata, int event, void *arg)
{
	int ndx;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)pdata->panel_private;
	struct panel_func *panel_func = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	panel_func = &vdd->panel_func;

	ndx = display_ndx_check(ctrl);

	if (ctrl->cmd_sync_wait_broadcast) {
		if (ctrl->cmd_sync_wait_trigger) {
			if(vdd->display_ststus_dsi[ndx].wait_disp_on) {
				mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_ON);
				vdd->display_ststus_dsi[ndx].wait_disp_on = 0;

		if(vdd->panel_func.samsung_backlight_late_on)
					vdd->panel_func.samsung_backlight_late_on(ctrl);

				if (vdd->dtsi_data[0].hmt_enabled) {
					if (!vdd->hmt_stat.hmt_is_first) {
						if (vdd->hmt_stat.hmt_on) {
							if (vdd->hmt_stat.hmt_low_persistence) {
								vdd->hmt_stat.hmt_enable(ctrl, 1);
								vdd->hmt_stat.hmt_reverse_update(ctrl, 1);
							}
							vdd->hmt_stat.hmt_bright_update(ctrl);
						}
					} else
						vdd->hmt_stat.hmt_is_first = 0;
				}
				LCD_INFO("DISPLAY_ON\n");
			}
		} else
			vdd->display_ststus_dsi[ndx].wait_disp_on = 0;
	} else {
		/* Check TE duration when the panel turned on */
		/*
		if (vdd->display_ststus_dsi[ctrl->ndx].wait_disp_on) {
			vdd->te_fitting_info.status &= ~TE_FITTING_DONE;
			vdd->te_fitting_info.te_duration = 0;
		}
		*/

		if (vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting &&
				!(vdd->te_fitting_info.status & TE_FITTING_DONE)) {
				if (panel_func->mdss_samsung_event_osc_te_fitting)
					panel_func->mdss_samsung_event_osc_te_fitting(pdata, event, arg);
		}

		if (vdd->display_ststus_dsi[ndx].wait_disp_on) {
			mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_ON);
			vdd->display_ststus_dsi[ndx].wait_disp_on = 0;

		if(vdd->panel_func.samsung_backlight_late_on)
				vdd->panel_func.samsung_backlight_late_on(ctrl);

			if (vdd->dtsi_data[0].hmt_enabled) {
				if (!vdd->hmt_stat.hmt_is_first) {
					if (vdd->hmt_stat.hmt_on) {
						if (vdd->hmt_stat.hmt_low_persistence) {
							vdd->hmt_stat.hmt_enable(ctrl, 1);
							vdd->hmt_stat.hmt_reverse_update(ctrl, 1);
						}
						vdd->hmt_stat.hmt_bright_update(ctrl);
					}
				} else
					vdd->hmt_stat.hmt_is_first = 0;
			}
			LCD_INFO("DISPLAY_ON\n");
		}
	}
}

static void mdss_samsung_event_fb_event_callback(struct mdss_panel_data *pdata, int event, void *arg)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct panel_func *panel_func = NULL;

	if (IS_ERR_OR_NULL(pdata)) {
		LCD_ERR("Invalid data pdata : 0x%zx\n", (size_t)pdata);
		return;
	}

	vdd = (struct samsung_display_driver_data *)pdata->panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return;
	}

	panel_func = &vdd->panel_func;

	if (IS_ERR_OR_NULL(panel_func)) {
		LCD_ERR("Invalid data panel_func : 0x%zx\n", (size_t)panel_func);
		return;
	}

	if (panel_func->mdss_samsung_event_esd_recovery_init)
		panel_func->mdss_samsung_event_esd_recovery_init(pdata, event, arg);
}


static int mdss_samsung_dsi_panel_event_handler(
		struct mdss_panel_data *pdata, int event, void *arg)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)pdata->panel_private;
	struct panel_func *panel_func = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return -EINVAL;
	}

	LCD_DEBUG("%d\n", event);

	panel_func = &vdd->panel_func;

	if (IS_ERR_OR_NULL(panel_func)) {
		LCD_ERR("Invalid data panel_func : 0x%zx\n", (size_t)panel_func);
		return -EINVAL;
	}

	switch (event) {
		case MDSS_SAMSUNG_EVENT_FRAME_UPDATE:
			if (!IS_ERR_OR_NULL(panel_func->mdss_samsung_event_frame_update))
				panel_func->mdss_samsung_event_frame_update(pdata, event, arg);
			break;
		case MDSS_SAMSUNG_EVENT_FB_EVENT_CALLBACK:
			if (!IS_ERR_OR_NULL(panel_func->mdss_samsung_event_fb_event_callback))
				panel_func->mdss_samsung_event_fb_event_callback(pdata, event, arg);
			break;
		default:
			LCD_DEBUG("unhandled event=%d\n", event);
			break;
	}

	return 0;
}

/*
 * Debugfs related functions
 */
static ssize_t mdss_samsung_dump_regs_debug(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	struct samsung_display_driver_data *vdd = file->private_data;

	if (IS_ERR_OR_NULL(vdd))
		return -EFAULT;

	mdss_samsung_read_rddpm();
	mdss_samsung_dump_regs();
	mdss_samsung_dsi_dump_regs(0);
	mdss_samsung_dsi_dump_regs(1);
	mdss_samsung_dsi_te_check();
	mdss_mdp_underrun_dump_info();

	return 0;
}

static struct file_operations panel_dump_ops = {
	.open = simple_open,
	.read = mdss_samsung_dump_regs_debug,
};

static void mdss_sasmung_panel_debug_create(struct samsung_display_driver_data *vdd)
{
	struct samsung_display_debug_data *debug_data;

	debug_data = vdd->debug_data;

	/* Create file on debugfs of display_driver */


	/* Create file on debugfs on dump */
	debugfs_create_file("reg_dump", 0600, debug_data->dump,
		vdd, &panel_dump_ops);
	debugfs_create_bool("print_cmds", 0600, debug_data->dump,
		(u32 *)&debug_data->print_cmds);
	debugfs_create_bool("panic_on_pptimeout", 0600, debug_data->dump,
		(u32 *)&debug_data->panic_on_pptimeout);

	/* Create file on debugfs on display_status */
	debugfs_create_u32("panel_attach_status", 0600, debug_data->display_status,
		(u32 *)&vdd->panel_attach_status);

	/* Create file on debugfs on hw_info */
	/* TBD */
}

static int mdss_sasmung_panel_debug_init(struct samsung_display_driver_data *vdd)
{
	struct samsung_display_debug_data *debug_data;
	static bool debugfs_init;
	int ret = 0;

	debug_data = kzalloc(sizeof(struct samsung_display_debug_data),
			GFP_KERNEL);

	if (IS_ERR_OR_NULL(vdd)) {
		ret = -ENODEV;
		goto end;
	}

	/*
	 * The debugfs must be init one time
	 * in case of dual dsi, this function will be called twice
	 */
	if (debugfs_init)
		goto end;

	debugfs_init = true;

	vdd->debug_data = debug_data;

	if (IS_ERR_OR_NULL(debug_data)) {
		LCD_ERR("no memory to create display debug data\n");
		ret = -ENOMEM;
		goto end;
	}

	/* INIT debug data */

	/*
	 * panic_on_pptimeout default value is false
	 * if you want to enable panic for specific project
	 * please change the value on your panel file.
	 * if you want to enable panic for all project
	 * please change the value here.
	 */
	debug_data->panic_on_pptimeout = false;

	/* Root directory for display driver */
	debug_data->root = debugfs_create_dir("display_driver", NULL);
	if (IS_ERR_OR_NULL(debug_data->root)) {
		LCD_ERR("debugfs_create_dir failed, error %ld(line:%d)\n", PTR_ERR(debug_data->root), __LINE__);
		ret = -ENODEV;
		goto end;
	}

	/* Directory for dump */
	debug_data->dump = debugfs_create_dir("dump", debug_data->root);
	if (IS_ERR_OR_NULL(debug_data->dump)) {
		LCD_ERR("debugfs_create_dir failed, error %ld(line:%d)\n", PTR_ERR(debug_data->dump), __LINE__);
		ret = -ENODEV;
		goto end;
	}

	/* Directory for hw_info */
	debug_data->hw_info = debugfs_create_dir("hw_info", debug_data->root);
	if (IS_ERR_OR_NULL(debug_data->root)) {
		LCD_ERR("debugfs_create_dir failed, error %ld(line:%d)\n", PTR_ERR(debug_data->root), __LINE__);
		ret = -ENODEV;
		goto end;
	}

	/* Directory for display_status */
	debug_data->display_status = debugfs_create_dir("display_status", debug_data->root);
	if (IS_ERR_OR_NULL(debug_data->display_status)) {
		LCD_ERR("debugfs_create_dir failed, error %ld(line:%d)\n", PTR_ERR(debug_data->root), __LINE__);
		ret = -ENODEV;
		goto end;
	}

	mdss_sasmung_panel_debug_create(vdd);

	if (ret)
		LCD_ERR("Fail to create files for debugfs\n");

end:
	if (ret && !IS_ERR_OR_NULL(debug_data->root))
			debugfs_remove_recursive(debug_data->root);

	return ret;
}

void mdss_samsung_panel_init(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	/* At this time ctrl_pdata->ndx is not set */
	struct mdss_panel_info *pinfo = NULL;
	int ndx = ctrl_pdata->panel_data.panel_info.pdest;
	int loop, loop2;

	ctrl_pdata->panel_data.panel_private = &vdd_data;

	if (mdss_sasmung_panel_debug_init(&vdd_data))
		LCD_ERR("Fail to create debugfs\n");

	mutex_init(&vdd_data.vdd_lock);
	/* To guarantee BLANK & UNBLANK mode change operation*/
	mutex_init(&vdd_data.vdd_blank_unblank_lock);

	vdd_data.ctrl_dsi[ndx] = ctrl_pdata;

	pinfo = &ctrl_pdata->panel_data.panel_info;

	/* Set default link_state of brightness command */
	for (loop = 0; loop < SUPPORT_PANEL_COUNT; loop++)
		vdd_data.brightness[loop].brightness_packet_tx_cmds_dsi.link_state = DSI_HS_MODE;

	if (pinfo && pinfo->mipi.mode == DSI_CMD_MODE) {
		vdd_data.panel_func.mdss_samsung_event_osc_te_fitting =
			mdss_samsung_event_osc_te_fitting;
	}

	vdd_data.panel_func.mdss_samsung_event_frame_update =
		mdss_samsung_event_frame_update;
	vdd_data.panel_func.mdss_samsung_event_fb_event_callback =
		mdss_samsung_event_fb_event_callback;
	vdd_data.panel_func.mdss_samsung_event_esd_recovery_init =
		mdss_samsung_event_esd_recovery_init;

	if (IS_ERR_OR_NULL(vdd_data.panel_func.samsung_panel_init))
		LCD_ERR("samsung_panel_init is error");
	else
		vdd_data.panel_func.samsung_panel_init(&vdd_data);

	vdd_data.ctrl_dsi[ndx]->event_handler = mdss_samsung_dsi_panel_event_handler;

	if (vdd_data.support_mdnie_lite || vdd_data.support_cabc)
		vdd_data.mdnie_tune_state_dsi[ndx] = init_dsi_tcon_mdnie_class(ndx, &vdd_data);

	/*
		Below for loop are same as initializing sturct brightenss_pasket_dsi.
	vdd_data.brightness[ndx].brightness_packet_dsi[0] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	vdd_data.brightness[ndx].brightness_packet_dsi[1] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	...
	vdd_data.brightness[ndx].brightness_packet_dsi[BRIGHTNESS_MAX_PACKET - 2] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	vdd_data.brightness[ndx].brightness_packet_dsi[BRIGHTNESS_MAX_PACKET - 1 ] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	*/
	for (loop = 0; loop < SUPPORT_PANEL_COUNT; loop++)
		for (loop2 = 0; loop2 < BRIGHTNESS_MAX_PACKET; loop2++)
			vdd_data.brightness[loop].brightness_packet_dsi[loop2] = default_cmd;

	for (loop = 0; loop < SUPPORT_PANEL_COUNT; loop++) {
		vdd_data.brightness[loop].brightness_packet_tx_cmds_dsi.cmds = &vdd_data.brightness[loop].brightness_packet_dsi[0];
		vdd_data.brightness[loop].brightness_packet_tx_cmds_dsi.cmd_cnt = 0;
	}

	spin_lock_init(&vdd_data.esd_recovery.irq_lock);

	vdd_data.hmt_stat.hmt_enable = hmt_enable;
	vdd_data.hmt_stat.hmt_reverse_update = hmt_reverse_update;
	vdd_data.hmt_stat.hmt_bright_update = hmt_bright_update;

	mdss_panel_attach_set(ctrl_pdata, true);

	/* Set init brightness level */
	vdd_data.bl_level = DEFAULT_BRIGHTNESS;

	/* Init blank_mode */
	for (loop = 0; loop < SUPPORT_PANEL_COUNT; loop++)
		vdd_data.vdd_blank_mode[loop] = FB_BLANK_POWERDOWN;

	/*Init Hall ic related things */
	mutex_init(&vdd_data.vdd_hall_ic_lock); // To guarantee HALL IC switching

}

void mdss_samsung_dsi_panel_registered(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return;
	}

	vdd->mfd_dsi[ctrl->ndx] = (struct msm_fb_data_type *)registered_fb[ctrl->ndx]->par;

	mdss_samsung_create_sysfs(vdd);
	LCD_INFO("DSI%d success", ctrl->ndx);
}

int mdss_samsung_parse_candella_lux_mapping_table(struct device_node *np,
		struct candella_lux_map *table, char *keystring)
{
	const __be32 *data;
	int  data_offset, len = 0 , i = 0;
	int  cdmap_start=0, cdmap_end=0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		LCD_DEBUG("%d, Unable to read table %s ", __LINE__, keystring);
		return -EINVAL;
	} else
		LCD_ERR("Success to read table %s\n", keystring);

	if ((len % 4) != 0) {
		LCD_ERR("%d, Incorrect table entries for %s", __LINE__, keystring);
		return -EINVAL;
	}

	table->lux_tab_size = len / (sizeof(int)*4);
	table->lux_tab = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
	if (!table->lux_tab)
		return -ENOMEM;

	table->cmd_idx = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
	if (!table->cmd_idx)
		goto error;

	data_offset = 0;
	for (i = 0 ; i < table->lux_tab_size; i++) {
		table->cmd_idx[i]= be32_to_cpup(&data[data_offset++]);	/* 1rst field => <idx> */
		cdmap_start = be32_to_cpup(&data[data_offset++]);		/* 2nd field => <from> */
		cdmap_end = be32_to_cpup(&data[data_offset++]);			/* 3rd field => <till> */
		table->lux_tab[i] = be32_to_cpup(&data[data_offset++]);	/* 4th field => <candella> */
		/* Fill the backlight level to lux mapping array */
		do{
			table->bkl[cdmap_start++] = i;
		}while(cdmap_start <= cdmap_end);
	}
	return 0;
error:
	kfree(table->lux_tab);

	return -ENOMEM;
}

int mdss_samsung_parse_hbm_candella_lux_mapping_table(struct device_node *np,
		struct hbm_candella_lux_map *table, char *keystring)
{
	const __be32 *data;
	int  data_offset, len = 0 , i = 0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		LCD_ERR("%d, Unable to read table %s ", __LINE__, keystring);
		return -EINVAL;
	} else
		LCD_ERR("Success to read table %s\n", keystring);

	if ((len % 4) != 0) {
		LCD_ERR("%d, Incorrect table entries for %s", __LINE__, keystring);
		return -EINVAL;
	}

	table->lux_tab_size = len / (sizeof(int)*5);

	table->lux_tab = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
	if (!table->lux_tab)
		return -ENOMEM;

	table->cmd_idx = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
	if (!table->cmd_idx)
		goto error;

	table->from = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
	if (!table->from)
		goto error;

	table->end = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
	if (!table->end)
		goto error;

	table->auto_level = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
	if (!table->auto_level)
		goto error;

	data_offset = 0;

	for (i = 0 ; i < table->lux_tab_size; i++) {
		table->cmd_idx[i] = be32_to_cpup(&data[data_offset++]);		/* 1st field => <idx> */
		table->from[i] = be32_to_cpup(&data[data_offset++]);		/* 2nd field => <from> */
		table->end[i] = be32_to_cpup(&data[data_offset++]);			/* 3rd field => <till> */
		table->lux_tab[i] = be32_to_cpup(&data[data_offset++]);		/* 4th field => <candella> */
		table->auto_level[i] = be32_to_cpup(&data[data_offset++]);  /* 5th field => <auto brightness level> */
	}

	table->hbm_min_lv = table->from[0];
	
	LCD_ERR("tab_size (%d) hbm_min_lv (%d)\n", table->lux_tab_size, table->hbm_min_lv);
			return 0;
error:
	kfree(table->lux_tab);
	kfree(table->cmd_idx);
	kfree(table->from);
	kfree(table->end);
	kfree(table->auto_level);

	return -ENOMEM;
}

int mdss_samsung_parse_panel_table(struct device_node *np,
		struct cmd_map *table, char *keystring)
{
	const __be32 *data;
	int  data_offset, len = 0 , i = 0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		LCD_DEBUG("%d, Unable to read table %s\n", __LINE__, keystring);
		return -EINVAL;
	} else
		LCD_ERR("Success to read table %s\n", keystring);

	if ((len % 2) != 0) {
		LCD_ERR("%d, Incorrect table entries for %s", __LINE__, keystring);
		return -EINVAL;
	}

	table->size = len / (sizeof(int)*2);
	table->bl_level = kzalloc((sizeof(int) * table->size), GFP_KERNEL);
	if (!table->bl_level)
		return -ENOMEM;

	table->cmd_idx = kzalloc((sizeof(int) * table->size), GFP_KERNEL);
	if (!table->cmd_idx)
		goto error;

	data_offset = 0;
	for (i = 0 ; i < table->size; i++) {
		table->bl_level[i] = be32_to_cpup(&data[data_offset++]);
		table->cmd_idx[i] = be32_to_cpup(&data[data_offset++]);
	}

	return 0;
error:
	kfree(table->cmd_idx);

	return -ENOMEM;
}

static int samsung_nv_read(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds, unsigned char *destBuffer)
{
	int loop_limit = 0;
	int read_pos = 0;
	int read_count = 0;
	int show_cnt;
	int i, j;
	char show_buffer[256] = {0,};
	int show_buffer_pos = 0;

	int read_size = 0;
	int srcLength = 0;
	int startoffset = 0;
	struct samsung_display_driver_data *vdd = NULL;
	int ndx = 0;
	int packet_size = 0;

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(cmds)) {
		LCD_ERR("Invalid ctrl data\n");
		return -EINVAL;
	}

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	ndx = display_ndx_check(ctrl);

	packet_size = vdd->dtsi_data[ndx].packet_size_tx_cmds[vdd->panel_revision].cmds->payload[0];

	srcLength = cmds->read_size[0];
	startoffset = read_pos = cmds->read_startoffset[0];

	show_buffer_pos += snprintf(show_buffer, sizeof(show_buffer), "read_reg : %X[%d] : ", cmds->cmds->payload[0], srcLength);

	loop_limit = (srcLength + packet_size - 1) / packet_size;
	mdss_samsung_send_cmd(ctrl, PANEL_PACKET_SIZE);

	show_cnt = 0;
	for (j = 0; j < loop_limit; j++) {
		vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[vdd->panel_revision].cmds->payload[1] = read_pos;
		read_size = ((srcLength - read_pos + startoffset) < packet_size) ?
					(srcLength - read_pos + startoffset) : packet_size;
		mdss_samsung_send_cmd(ctrl, PANEL_REG_READ_POS);

		mutex_lock(&vdd->vdd_lock);
		read_count = mdss_samsung_panel_cmd_read(ctrl, cmds, read_size);
		mutex_unlock(&vdd->vdd_lock);

		for (i = 0; i < read_count; i++, show_cnt++) {
			show_buffer_pos += snprintf(show_buffer + show_buffer_pos, sizeof(show_buffer)-show_buffer_pos, "%02x ",
					ctrl->rx_buf.data[i]);
			if (destBuffer != NULL && show_cnt < srcLength) {
					destBuffer[show_cnt] =
					ctrl->rx_buf.data[i];
			}
		}

		show_buffer_pos += snprintf(show_buffer + show_buffer_pos, sizeof(show_buffer)-show_buffer_pos, ".");
		read_pos += read_count;

		if (read_pos-startoffset >= srcLength)
			break;
	}

	LCD_ERR("mdss DSI%d %s\n", ndx, show_buffer);

	return read_pos-startoffset;
}

struct dsi_panel_cmds *mdss_samsung_cmds_select(struct mdss_dsi_ctrl_pdata *ctrl, enum mipi_samsung_cmd_list cmd)
{
	int ndx;
	struct dsi_panel_cmds *cmds = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	if (IS_ERR_OR_NULL(vdd))
		return NULL;

	ndx = display_ndx_check(ctrl);

	switch (cmd) {
		case PANEL_DISPLAY_ON:
			cmds = &vdd->dtsi_data[ndx].display_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_DISPLAY_OFF:
			cmds = &vdd->dtsi_data[ndx].display_off_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_BRIGHT_CTRL:
			cmds = &vdd->brightness[ndx].brightness_packet_tx_cmds_dsi;
			break;
		case PANEL_LEVE1_KEY_ENABLE:
			cmds = &vdd->dtsi_data[ndx].level1_key_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LEVE1_KEY_DISABLE:
			cmds = &vdd->dtsi_data[ndx].level1_key_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LEVE2_KEY_ENABLE:
			cmds = &vdd->dtsi_data[ndx].level2_key_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LEVE2_KEY_DISABLE:
			cmds = &vdd->dtsi_data[ndx].level2_key_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_PACKET_SIZE:
			cmds = &vdd->dtsi_data[ndx].packet_size_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_REG_READ_POS:
			cmds = &vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_MDNIE_TUNE:
			cmds = &vdd->mdnie_tune_data[ndx].mdnie_tune_packet_tx_cmds_dsi;
			break;
		case PANEL_OSC_TE_FITTING:
			cmds = &vdd->dtsi_data[ndx].osc_te_fitting_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_AVC_ON:
			cmds = &vdd->dtsi_data[ndx].avc_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LDI_FPS_CHANGE:
			cmds = &vdd->dtsi_data[ndx].ldi_fps_change_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_ENABLE:
			cmds = &vdd->dtsi_data[ndx].hmt_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_DISABLE:
			cmds = &vdd->dtsi_data[ndx].hmt_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_REVERSE_ENABLE:
			cmds = &vdd->dtsi_data[ndx].hmt_reverse_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_REVERSE_DISABLE:
			cmds = &vdd->dtsi_data[ndx].hmt_reverse_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LDI_SET_VDD_OFFSET:
			cmds = &vdd->dtsi_data[ndx].write_vdd_offset_cmds[vdd->panel_revision];
			break;
		case PANEL_LDI_SET_VDDM_OFFSET:
			cmds = &vdd->dtsi_data[ndx].write_vddm_offset_cmds[vdd->panel_revision];
			break;
		case PANEL_ALPM_ON:
			cmds = &vdd->dtsi_data[ndx].alpm_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_ALPM_OFF:
			cmds = &vdd->dtsi_data[ndx].alpm_off_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HSYNC_ON:
			cmds = &vdd->dtsi_data[ndx].hsync_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_CABC_ON:
			cmds = &vdd->dtsi_data[ndx].cabc_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_CABC_OFF:
			cmds = &vdd->dtsi_data[ndx].cabc_off_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_BLIC_DIMMING:
			cmds = &vdd->dtsi_data[ndx].blic_dimming_cmds[vdd->panel_revision];
			break;
		case PANEL_CABC_ON_DUTY:
			cmds = &vdd->dtsi_data[ndx].cabc_on_duty_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_CABC_OFF_DUTY:
			cmds = &vdd->dtsi_data[ndx].cabc_off_duty_tx_cmds[vdd->panel_revision];
			break;
		default:
			LCD_ERR("unknown_command.. \n");
			break;
	}

	return cmds;
}

int mdss_samsung_send_cmd(struct mdss_dsi_ctrl_pdata *ctrl, enum mipi_samsung_cmd_list cmd)
{
	struct mdss_panel_info *pinfo = NULL;
	struct samsung_display_driver_data *vdd = NULL;

	if (!mdss_panel_attach_get(ctrl)) {
		LCD_ERR("mdss_panel_attach_get(%d) : %d\n", ctrl->ndx, mdss_panel_attach_get(ctrl));
		return -EAGAIN;
	}

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is null\n");
		return 0;
	}

	pinfo = &(ctrl->panel_data.panel_info);
	if ((pinfo->blank_state == MDSS_PANEL_BLANK_BLANK) || (vdd->panel_func.samsung_lvds_write_reg)) {
		LCD_ERR("blank stste is (%d)\n", pinfo->blank_state);
		return 0;
	}

	/* To check registered FB */
	if (IS_ERR_OR_NULL(vdd->mfd_dsi[ctrl->ndx])) {
		/* Do not send any CMD data under FB_BLANK_POWERDOWN condition*/
		if (vdd->vdd_blank_mode[DISPLAY_1] == FB_BLANK_POWERDOWN) {
			LCD_ERR("fb blank is POWERDOWN mode\n");
			return 0;
		}
	} else {
		/* Do not send any CMD data under FB_BLANK_POWERDOWN condition*/
		if (vdd->vdd_blank_mode[ctrl->ndx] == FB_BLANK_POWERDOWN) {
			LCD_ERR("fb blank is POWERDOWN mode.\n");
			return 0;
		}
	}

	mutex_lock(&vdd->vdd_lock);
	mdss_dsi_panel_cmds_send(ctrl, mdss_samsung_cmds_select(ctrl, cmd));
	mutex_unlock(&vdd->vdd_lock);

	return 0;
}

static int mdss_samsung_read_nv_mem(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds, unsigned char *buffer, int level_key)
{
	int nv_read_cnt = 0;

	if (level_key & PANEL_LEVE1_KEY)
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE1_KEY_ENABLE);
	if (level_key & PANEL_LEVE2_KEY)
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE2_KEY_ENABLE);

	nv_read_cnt = samsung_nv_read(ctrl, cmds, buffer);

	if (level_key & PANEL_LEVE1_KEY)
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE1_KEY_DISABLE);
	if (level_key & PANEL_LEVE2_KEY)
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE2_KEY_DISABLE);

	return nv_read_cnt;
}

void mdss_samsung_panel_data_read(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds, char *buffer, int level_key)
{
	if (!ctrl) {
		LCD_ERR("Invalid ctrl data\n");
		return;
	}

	if (!mdss_panel_attach_get(ctrl)) {
		LCD_ERR("mdss_panel_attach_get(%d) : %d\n", ctrl->ndx, mdss_panel_attach_get(ctrl));
		return;
	}

	if (!cmds->cmd_cnt) {
		LCD_ERR("cmds_count is zero..\n");
		return;
	}

	mdss_samsung_read_nv_mem(ctrl, cmds, buffer, level_key);
}

static unsigned char mdss_samsung_manufacture_id(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds)
{
	char manufacture_id = 0;

	mdss_samsung_panel_data_read(ctrl, cmds, &manufacture_id, PANEL_LEVE1_KEY);

	return manufacture_id;
}

int mdss_samsung_panel_on_pre(struct mdss_panel_data *pdata)
{
	int ndx;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	ndx = display_ndx_check(ctrl);

	vdd->display_ststus_dsi[ndx].disp_on_pre = 1;

#if !defined(CONFIG_SEC_FACTORY)
	/* LCD ID read every wake_up time incase of factory binary */
	if(vdd->dtsi_data[ndx].tft_common_support)
		return false;
#endif

	if (!mdss_panel_attach_get(ctrl)) {
		LCD_ERR("mdss_panel_attach_get(%d) : %d\n", ndx, mdss_panel_attach_get(ctrl));
		return false;
	}

	LCD_INFO("+: ndx=%d\n", ndx);

#if defined(CONFIG_SEC_FACTORY)
	/* LCD ID read every wake_up time incase of factory binary */
	vdd->manufacture_id_dsi[ctrl->ndx] = 0;
#endif

	if (!vdd->manufacture_id_dsi[ndx]) {
		/*
		*	At this time, panel revision it not selected.
		*	So last index(SUPPORT_PANEL_REVISION-1) used.
		*/
		vdd->panel_revision = SUPPORT_PANEL_REVISION-1;

		/*
		*	Some panel needs to update register at init time to read ID & MTP
		*	Such as, dual-dsi-control or sleep-out so on.
		*/
		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].manufacture_read_pre_tx_cmds[vdd->panel_revision].cmds)) {
			mutex_lock(&vdd->vdd_lock);
			mdss_dsi_panel_cmds_send(ctrl, &vdd->dtsi_data[ndx].manufacture_read_pre_tx_cmds[vdd->panel_revision]);
			mutex_unlock(&vdd->vdd_lock);
			LCD_ERR("DSI%d manufacture_read_pre_tx_cmds ", ndx);
		}

		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].manufacture_id0_rx_cmds[vdd->panel_revision].cmds)) {
			vdd->manufacture_id_dsi[ndx] = mdss_samsung_manufacture_id(ctrl, &vdd->dtsi_data[ndx].manufacture_id0_rx_cmds[SUPPORT_PANEL_REVISION-1]);
			vdd->manufacture_id_dsi[ndx] <<= 8;
		} else
			LCD_ERR("DSI%d manufacture_id0_rx_cmds NULL", ndx);

		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].manufacture_id1_rx_cmds[vdd->panel_revision].cmds)) {
			vdd->manufacture_id_dsi[ndx] |= mdss_samsung_manufacture_id(ctrl, &vdd->dtsi_data[ndx].manufacture_id1_rx_cmds[SUPPORT_PANEL_REVISION-1]);
			vdd->manufacture_id_dsi[ndx] <<= 8;
		} else
			LCD_ERR("DSI%d manufacture_id1_rx_cmds NULL", ndx);

		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].manufacture_id2_rx_cmds[vdd->panel_revision].cmds))
			vdd->manufacture_id_dsi[ndx] |= mdss_samsung_manufacture_id(ctrl, &vdd->dtsi_data[ndx].manufacture_id2_rx_cmds[SUPPORT_PANEL_REVISION-1]);
		else
			LCD_ERR("DSI%d manufacture_id2_rx_cmds NULL", ndx);


		LCD_INFO("DSI%d manufacture_id=0x%x\n", ndx, vdd->manufacture_id_dsi[ndx]);

		/* Panel revision selection */
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_revision))
			LCD_ERR("DSI%d no panel_revision_selection_error fucntion\n", ndx);
		else
			vdd->panel_func.samsung_panel_revision(ctrl);

		LCD_INFO("DSI%d Panel_Revision = %c %d\n", ndx, vdd->panel_revision + 'A', vdd->panel_revision);
	}

	/* Manufacture date */
	if (!vdd->manufacture_date_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_manufacture_date_read))
			LCD_ERR("DSI%d manufacture_date_error\n", ndx);
		else
			vdd->manufacture_date_loaded_dsi[ndx] = vdd->panel_func.samsung_manufacture_date_read(ctrl);
	}

	/* DDI ID */
	if (!vdd->ddi_id_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_ddi_id_read))
			LCD_ERR("DSI%d ddi_id_error\n", ndx);
		else
			vdd->ddi_id_loaded_dsi[ndx] = vdd->panel_func.samsung_ddi_id_read(ctrl);
	}

	/* HBM */
	if (!vdd->hbm_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_hbm_read))
			LCD_ERR("DSI%d HBM error\n", ndx);
		else
			vdd->hbm_loaded_dsi[ndx] = vdd->panel_func.samsung_hbm_read(ctrl);
	}

	/* MDNIE X,Y */
	if (!vdd->mdnie_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_mdnie_read))
			LCD_ERR("DSI%d mdnie_x_y_error\n", ndx);
		else
			vdd->mdnie_loaded_dsi[ndx] = vdd->panel_func.samsung_mdnie_read(ctrl);
	}

	/* Panel Unique Cell ID */
	if (!vdd->cell_id_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_cell_id_read))
			LCD_ERR("DSI%d cell_id_error\n", ndx);
		else {
			vdd->cell_id_loaded_dsi[ndx] = vdd->panel_func.samsung_cell_id_read(ctrl);
		}
	}

	/* Smart dimming*/
	if (!vdd->smart_dimming_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_smart_dimming_init))
			LCD_ERR("DSI%d smart dimming error\n", ndx);
		else
			vdd->smart_dimming_loaded_dsi[ndx] = vdd->panel_func.samsung_smart_dimming_init(ctrl);
	}

	/* Smart dimming for hmt */
	if (vdd->dtsi_data[0].hmt_enabled) {
		if (!vdd->smart_dimming_hmt_loaded_dsi[ndx]) {
			if (IS_ERR_OR_NULL(vdd->panel_func.samsung_smart_dimming_hmt_init))
				LCD_ERR("DSI%d smart dimming hmt init error\n", ndx);
			else
				vdd->smart_dimming_hmt_loaded_dsi[ndx] = vdd->panel_func.samsung_smart_dimming_hmt_init(ctrl);
		}
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_on_pre))
		vdd->panel_func.samsung_panel_on_pre(ctrl);

	LCD_INFO("-: ndx=%d\n", ndx);

	return true;
}

int mdss_samsung_panel_on_post(struct mdss_panel_data *pdata)
{
	int ndx;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_panel_info *pinfo = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(ctrl)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (!mdss_panel_attach_get(ctrl)) {
		LCD_ERR("mdss_panel_attach_get(%d) : %d\n", ctrl->ndx, mdss_panel_attach_get(ctrl));
		return false;
	}

	pinfo = &(ctrl->panel_data.panel_info);

	if (IS_ERR_OR_NULL(pinfo)) {
		LCD_ERR("Invalid data pinfo : 0x%zx\n", (size_t)pinfo);
		return false;
	}

	ndx = display_ndx_check(ctrl);

	LCD_INFO("+: ndx=%d\n", ndx);

	if(vdd->support_cabc && !vdd->auto_brightness)
		mdss_samsung_cabc_update();
	else if(vdd->mdss_panel_tft_outdoormode_update && vdd->auto_brightness )
		vdd->mdss_panel_tft_outdoormode_update(ctrl);
	else if(vdd->support_cabc && vdd->auto_brightness)
		mdss_tft_autobrightness_cabc_update(ctrl);

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_on_post))
		vdd->panel_func.samsung_panel_on_post(ctrl);

	/* Recovery Mode : Set some default brightness */
	if (vdd->recovery_boot_mode)
		vdd->bl_level = DEFAULT_BRIGHTNESS;

	mdss_samsung_update_brightness_value();
	
	if ((vdd->ctrl_dsi[DISPLAY_1]->bklt_ctrl == BL_DCS_CMD))
		mdss_samsung_brightness_dcs(ctrl, vdd->bl_level);

	if (vdd->support_mdnie_lite)
		update_dsi_tcon_mdnie_register(vdd);

	vdd->display_ststus_dsi[ndx].wait_disp_on = true;

	if (pinfo->esd_check_enabled) {
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);
		vdd->esd_recovery.is_enabled_esd_recovery = true;
	}
	LCD_INFO("-: ndx=%d\n", ndx);

	return true;
}

int mdss_samsung_panel_off_pre(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_off_pre))
		vdd->panel_func.samsung_panel_off_pre(ctrl);

	return ret;
}

int mdss_samsung_panel_off_post(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;
	struct mdss_panel_info *pinfo = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	pinfo = &(ctrl->panel_data.panel_info);

	if (IS_ERR_OR_NULL(pinfo)) {
		LCD_ERR("Invalid data pinfo : 0x%zx\n", (size_t)pinfo);
		return false;
	}

	if (pinfo->esd_check_enabled) {
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);
		vdd->esd_recovery.is_enabled_esd_recovery = false;
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_off_post))
		vdd->panel_func.samsung_panel_off_post(ctrl);

	return ret;
}

/*************************************************************
*
*		EXTRA POWER RELATED FUNCTION BELOW.
*
**************************************************************/
static int mdss_dsi_extra_power_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0, i;
	/*
	 * gpio_name[] named as gpio_name + num(recomend as 0)
	 * because of the num will increase depend on number of gpio
	 */
	static const char gpio_name[] = "panel_extra_power";
	static u8 gpio_request_status = -EINVAL;
	struct samsung_display_driver_data *vdd = NULL;

	if (!gpio_request_status)
		goto end;

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)){
		LCD_ERR("Invalid data pinfo : 0x%zx\n", (size_t)vdd);
		goto end;
	}

	for (i = 0; i < MAX_EXTRA_POWER_GPIO; i++) {
		if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]) && mdss_panel_attach_get(ctrl)) {
			rc = gpio_request(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i],
							gpio_name);
			if (rc) {
				LCD_ERR("request %s failed, rc=%d\n", gpio_name, rc);
				goto extra_power_gpio_err;
			}
		}
	}

	gpio_request_status = rc;
end:
	return rc;
extra_power_gpio_err:
	if (i) {
		do {
			if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]))
				gpio_free(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i--]);
			LCD_ERR("i = %d\n", i);
		} while (i > 0);
	}

	return rc;
}

int mdss_samsung_panel_extra_power(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	int ret = 0, i = 0, add_value = 1;
	struct samsung_display_driver_data *vdd = NULL;

	if (IS_ERR_OR_NULL(pdata)) {
		LCD_ERR("Invalid pdata : 0x%zx\n", (size_t)pdata);
		ret = -EINVAL;
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)){
		LCD_ERR("Invalid data pinfo : 0x%zx\n", (size_t)vdd);
		goto end;
	}

	LCD_INFO("++ enable(%d) ndx(%d)\n", enable, ctrl->ndx);

	if (ctrl->ndx == DSI_CTRL_1)
		goto end;

	if (mdss_dsi_extra_power_request_gpios(ctrl)) {
		LCD_ERR("fail to request extra power gpios");
		goto end;
	}

	LCD_DEBUG("%s extra power gpios\n", enable ? "enable" : "disable");

	/*
	 * The order of extra_power_gpio enable/disable
	 * 1. Enable : panel_extra_power_gpio[0], [1], ... [MAX_EXTRA_POWER_GPIO - 1]
	 * 2. Disable : panel_extra_power_gpio[MAX_EXTRA_POWER_GPIO - 1], ... [1], [0]
	 */
	if (!enable) {
		add_value = -1;
		i = MAX_EXTRA_POWER_GPIO - 1;
	}

	do {
		if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]) && mdss_panel_attach_get(ctrl)) {
			gpio_set_value(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i], enable);
			LCD_DEBUG("set extra power gpio[%d] to %s\n",
						 vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i],
						enable ? "high" : "low");
			usleep_range(1500, 1500);
		}
	} while (((i += add_value) < MAX_EXTRA_POWER_GPIO) && (i >= 0));

end:
	LCD_INFO("--\n");
	return ret;
}
/*************************************************************
*
*		TFT BACKLIGHT GPIO FUNCTION BELOW.
*
**************************************************************/
int mdss_backlight_tft_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0, i;
	/*
	 * gpio_name[] named as gpio_name + num(recomend as 0)
	 * because of the num will increase depend on number of gpio
	 */
	char gpio_name[17] = "disp_bcklt_gpio0";
	static u8 gpio_request_status = -EINVAL;
	struct samsung_display_driver_data *vdd = NULL;

	if (!gpio_request_status)
		goto end;

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)){
		LCD_ERR("Invalid data pinfo : 0x%zx\n", (size_t)vdd);
		goto end;
	}

	for (i = 0; i < MAX_BACKLIGHT_TFT_GPIO; i++) {
		if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i])) {
			rc = gpio_request(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i],
							gpio_name);
			if (rc) {
				LCD_ERR("request %s failed, rc=%d\n", gpio_name, rc);
				goto tft_backlight_gpio_err;
			}
		}
	}

	gpio_request_status = rc;
end:
	return rc;
tft_backlight_gpio_err:
	if (i) {
		do {
			if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i]))
				gpio_free(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i--]);
			LCD_ERR("i = %d\n", i);
		} while (i > 0);
	}

	return rc;
}
int mdss_backlight_tft_gpio_config(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	int ret = 0, i = 0, add_value = 1;
	struct samsung_display_driver_data *vdd = NULL;

	if (IS_ERR_OR_NULL(pdata)) {
		LCD_ERR("Invalid pdata : 0x%zx\n", (size_t)pdata);
		ret = -EINVAL;
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)){
		LCD_ERR("Invalid data pinfo : 0x%zx\n", (size_t)vdd);
		goto end;
	}

	LCD_INFO("++ enable(%d) ndx(%d)\n", enable, ctrl->ndx);

	if (ctrl->ndx == DSI_CTRL_1)
		goto end;

	if (mdss_backlight_tft_request_gpios(ctrl)) {
		LCD_ERR("fail to request tft backlight gpios");
		goto end;
	}

	LCD_DEBUG("%s tft backlight gpios\n", enable ? "enable" : "disable");

	/*
	 * The order of backlight_tft_gpio enable/disable
	 * 1. Enable : backlight_tft_gpio[0], [1], ... [MAX_BACKLIGHT_TFT_GPIO - 1]
	 * 2. Disable : backlight_tft_gpio[MAX_BACKLIGHT_TFT_GPIO - 1], ... [1], [0]
	 */
	if (!enable) {
		add_value = -1;
		i = MAX_BACKLIGHT_TFT_GPIO - 1;
	}

	do {
		if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i])) {
			gpio_set_value(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i], enable);
			LCD_DEBUG("set backlight tft gpio[%d] to %s\n",
						 vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i],
						enable ? "high" : "low");
			usleep_range(500, 500);
		}
	} while (((i += add_value) < MAX_BACKLIGHT_TFT_GPIO) && (i >= 0));

end:
	LCD_INFO("--\n");
	return ret;
}

/*************************************************************
*
*		ESD RECOVERY RELATED FUNCTION BELOW.
*
**************************************************************/

static int mdss_dsi_esd_irq_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	LCD_INFO("lcd esd recovery\n");

	return !ctrl->status_value;
}

/*
 * esd_irq_enable() - Enable or disable esd irq.
 *
 * @enable	: flag for enable or disabled
 * @nosync	: flag for disable irq with nosync
 * @data	: point ot struct mdss_panel_info
 */
static void esd_irq_enable(bool enable, bool nosync, void *data)
{
	/* The irq will enabled when do the request_threaded_irq() */
	static bool is_enabled = true;
	int gpio;
	unsigned long flags;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)data;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return;
	}

	spin_lock_irqsave(&vdd->esd_recovery.irq_lock, flags);
	gpio = vdd->esd_recovery.esd_gpio;

	if (enable == is_enabled) {
		LCD_INFO("ESD irq already %s\n", enable ? "enabled" : "disabled");
		goto error;
	}

	if (enable) {
		is_enabled = true;
		enable_irq(gpio_to_irq(gpio));
	} else {
		if (nosync)
			disable_irq_nosync(gpio_to_irq(gpio));
		else
			disable_irq(gpio_to_irq(gpio));
		is_enabled = false;
	}

	/* TODO: Disable log if the esd function stable */
	LCD_DEBUG("ESD irq %s with %s\n",
				enable ? "enabled" : "disabled",
				nosync ? "nosync" : "sync");
error:
	spin_unlock_irqrestore(&vdd->esd_recovery.irq_lock, flags);

}

static irqreturn_t esd_irq_handler(int irq, void *handle)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;

	if (!handle) {
		LCD_INFO("handle is null\n");
		return IRQ_HANDLED;
	}

	ctrl = (struct mdss_dsi_ctrl_pdata *)handle;

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx\n", (size_t)vdd);
		goto end;
	}

	if (!vdd->esd_recovery.is_enabled_esd_recovery) {
		LCD_DEBUG("esd recovery is not enabled yet");
		goto end;
	}
	LCD_INFO("++\n");

	esd_irq_enable(false, true, (void *)vdd);

	schedule_work(&pstatus_data->check_status.work);
	LCD_INFO("--\n");

end:
	return IRQ_HANDLED;
}

static void mdss_samsung_event_esd_recovery_init(struct mdss_panel_data *pdata, int event, void *arg)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	int ret;
	uint32_t *interval;
	uint32_t interval_ms_for_irq = 500;
	struct mdss_panel_info *pinfo;

	if (IS_ERR_OR_NULL(pdata)) {
		LCD_ERR("Invalid pdata : 0x%zx\n", (size_t)pdata);
		return;
	}

	vdd = (struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	interval = arg;

	pinfo = &ctrl->panel_data.panel_info;

	if (IS_ERR_OR_NULL(ctrl) ||	IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return;
	}
	if (unlikely(!vdd->esd_recovery.esd_recovery_init)) {
		vdd->esd_recovery.esd_recovery_init = true;
		vdd->esd_recovery.esd_irq_enable = esd_irq_enable;
		if (ctrl->status_mode == ESD_REG_IRQ) {
			if (gpio_is_valid(vdd->esd_recovery.esd_gpio)) {
				gpio_request(vdd->esd_recovery.esd_gpio, "esd_recovery");
				ret = request_threaded_irq(
						gpio_to_irq(vdd->esd_recovery.esd_gpio),
						NULL,
						esd_irq_handler,
						vdd->esd_recovery.irqflags,
						"esd_recovery",
						(void *)ctrl);
				if (ret)
					LCD_ERR("Failed to request_irq, ret=%d\n", ret);
				else
					esd_irq_enable(false, true, (void *)vdd);
				*interval = interval_ms_for_irq;
			}
		}
	}
}

/*************************************************************
*
*		BRIGHTNESS RELATED FUNCTION BELOW.
*
**************************************************************/
int get_cmd_index(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].bkl[vdd->bl_level];

	if (vdd->dtsi_data[0].hmt_enabled && vdd->hmt_stat.hmt_on)
		index = vdd->cmd_idx;

	return vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].cmd_idx[index];
}

int get_candela_value(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].bkl[vdd->bl_level];

	if (vdd->dtsi_data[0].hmt_enabled && vdd->hmt_stat.hmt_on)
		index = vdd->cmd_idx;

	return vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].lux_tab[index];
}

void set_auto_brightness_value(struct samsung_display_driver_data *vdd, int ndx)
{
	int i, from, end;
	int size;

	size = vdd->dtsi_data[ndx].hbm_candela_map_table[vdd->panel_revision].lux_tab_size;

	for (i=0; i<size; i++) {
		from = vdd->dtsi_data[ndx].hbm_candela_map_table[vdd->panel_revision].from[i];
		end = vdd->dtsi_data[ndx].hbm_candela_map_table[vdd->panel_revision].end[i];

		if (vdd->bl_level >= from && vdd->bl_level <= end)
			break;
	}

	if (i == size) {
		LCD_ERR("can not find auto brightness value !!(for %d / size %d)\n", vdd->bl_level, size);
		i = size-1;
	}

	vdd->candela_level = vdd->dtsi_data[ndx].hbm_candela_map_table[vdd->panel_revision].lux_tab[i];
	vdd->auto_brightness = vdd->dtsi_data[ndx].hbm_candela_map_table[vdd->panel_revision].auto_level[i];
	return;
}

int get_scaled_level(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].scaled_level_map_table[vdd->panel_revision].bkl[vdd->bl_level];

	return vdd->dtsi_data[ndx].scaled_level_map_table[vdd->panel_revision].lux_tab[index];
}

static void mdss_samsung_update_brightness_packet(struct dsi_cmd_desc *packet, int *count, struct dsi_panel_cmds *tx_cmd)
{
	int loop = 0;

	if (IS_ERR_OR_NULL(packet)) {
		LCD_ERR("%ps packet error", __builtin_return_address(0));
		return;
	}

	if (IS_ERR_OR_NULL(tx_cmd)) {
		LCD_ERR("%ps tx_cmd error", __builtin_return_address(0));
		return;
	}

	if (*count > (BRIGHTNESS_MAX_PACKET - 1))/*cmd_count is index, if cmd_count >13 then panic*/
		panic("over max brightness_packet size(%d).. !!", BRIGHTNESS_MAX_PACKET);

	for (loop = 0; loop < tx_cmd->cmd_cnt; loop++) {
		packet[*count].dchdr.dtype = tx_cmd->cmds[loop].dchdr.dtype;
		packet[*count].dchdr.wait = tx_cmd->cmds[loop].dchdr.wait;
		packet[*count].dchdr.dlen = tx_cmd->cmds[loop].dchdr.dlen;

		packet[*count].payload = tx_cmd->cmds[loop].payload;

		(*count)++;
	}
}

static void update_packet_level_key_enable(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *packet, int *cmd_cnt, int level_key)
{
	if (!level_key)
		return;
	else {
		if (level_key & PANEL_LEVE1_KEY)
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE1_KEY_ENABLE));

		if (level_key & PANEL_LEVE2_KEY)
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE2_KEY_ENABLE));

	}
}

static void update_packet_level_key_disable(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *packet, int *cmd_cnt, int level_key)
{
	if (!level_key)
		return;
	else {
		if (level_key & PANEL_LEVE1_KEY)
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE1_KEY_DISABLE));

		if (level_key & PANEL_LEVE2_KEY)
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE2_KEY_DISABLE));

	}
}

int mdss_samsung_hbm_brightenss_packet_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ndx;
	int cmd_cnt = 0;
	int level_key = 0;
	struct dsi_cmd_desc *packet = NULL;
	struct dsi_panel_cmds *tx_cmd = NULL;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	ndx = display_ndx_check(ctrl);

	if (vdd->display_ststus_dsi[ndx].hbm_mode) {
		LCD_ERR("DSI%d: already hbm mode! return ", ndx);
		return 0;
	}

	/* init packet */
	packet = &vdd->brightness[ndx].brightness_packet_dsi[0];

	/*
	*	HBM doesn't need calculated cmds. So Just use previously fixed data.
	*/
	/* To check supporting HBM mdoe by hbm_gamma_tx_cmds */
	if (vdd->dtsi_data[ndx].hbm_gamma_tx_cmds[vdd->panel_revision].cmd_cnt) {
		/* hbm gamma */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_hbm_gamma)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_hbm_gamma(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	if (vdd->dtsi_data[ndx].hbm_etc_tx_cmds[vdd->panel_revision].cmd_cnt) {
		/* hbm etc */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_hbm_etc)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_hbm_etc(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	return cmd_cnt;
}

int mdss_samsung_normal_brightenss_packet_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ndx;
	int cmd_cnt = 0;
	int level_key = 0;
	struct dsi_cmd_desc *packet = NULL;
	struct dsi_panel_cmds *tx_cmd = NULL;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	ndx = display_ndx_check(ctrl);

	packet = &vdd->brightness[ndx].brightness_packet_dsi[0]; /* init packet */

	if (vdd->smart_dimming_loaded_dsi[ndx]) { /* OCTA PANEL */
		/* hbm off */
		if (vdd->display_ststus_dsi[ndx].hbm_mode) {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_hbm_off)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_hbm_off(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
		}

		/* aid/aor */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_aid)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_aid(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* For no ACL setting menu Project, but need to  disable ACL in
			max brightness and enable acl in other brightness*/
		if (vdd->dtsi_data[ndx].samsung_change_acl_by_brightness) {
			if(vdd->bl_level<255)
				vdd->acl_status=1;
			else
				vdd->acl_status=0;
		}

		/* acl */
		if (vdd->acl_status || vdd->siop_status) {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_acl_on)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_acl_on(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}

			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_acl_percent)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_acl_percent(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
		} else {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_acl_off)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_acl_off(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
		}

		/* elvss */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* temperature elvss */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss_temperature1)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss_temperature1(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss_temperature2)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss_temperature2(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* vint */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_vint)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_vint(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* gamma */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_gamma)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_gamma(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	} else { /* TFT PANEL */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_tft_pwm_ldi)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_tft_pwm_ldi(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	return cmd_cnt;
}

static int mdss_samsung_single_transmission_packet(struct samsung_brightenss_data *tx_packet)
{
	int loop;
	struct dsi_cmd_desc *packet = tx_packet->brightness_packet_dsi;
	int packet_cnt = tx_packet->brightness_packet_tx_cmds_dsi.cmd_cnt;

	for(loop = 0; (loop < packet_cnt) && (loop < BRIGHTNESS_MAX_PACKET); loop++) {
		if (packet[loop].dchdr.dtype == DTYPE_DCS_LWRITE ||
			packet[loop].dchdr.dtype == DTYPE_GEN_LWRITE)
			packet[loop].dchdr.last = 0;
		else {
			if (loop > 0)
				packet[loop - 1].dchdr.last = 1; /*To ensure previous single tx packet */

			packet[loop].dchdr.last = 1;
		}
	}

	if (loop == BRIGHTNESS_MAX_PACKET)
		return false;
	else {
		packet[loop - 1].dchdr.last = 1; /* To make last packet flag */
		return true;
	}
}

int mdss_samsung_brightness_dcs(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct samsung_display_driver_data *vdd = NULL;
	int cmd_cnt = 0;
	int ret = 0;
	int ndx;

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (!get_lcd_attached("GET"))
		return false;

	vdd->bl_level = level;

	if (vdd->dtsi_data[0].hmt_enabled && vdd->hmt_stat.hmt_on) {
		LCD_ERR("HMT is on. do not set normal brightness..(%d)\n", level);
		return false;
	}

	if(alpm_status_func(CHECK_PREVIOUS_STATUS)){
		LCD_INFO("ALPM is on. do not set brightness..\n");
		return false;
	}

	ndx = display_ndx_check(ctrl);

	if (vdd->auto_brightness >= HBM_MODE && vdd->bl_level == 255 && !vdd->dtsi_data[ndx].tft_common_support) {
		cmd_cnt = mdss_samsung_hbm_brightenss_packet_set(ctrl);
		cmd_cnt > 0 ? vdd->display_ststus_dsi[ndx].hbm_mode = true : false;
	} else {
		cmd_cnt = mdss_samsung_normal_brightenss_packet_set(ctrl);
		cmd_cnt > 0 ? vdd->display_ststus_dsi[ndx].hbm_mode = false : false;
	}

	if (cmd_cnt) {
		/* setting tx cmds cmt */
		vdd->brightness[ndx].brightness_packet_tx_cmds_dsi.cmd_cnt = cmd_cnt;

		/* generate single tx packet */
		ret = mdss_samsung_single_transmission_packet(&vdd->brightness[ndx]);

		/* sending tx cmds */
		if (ret) {
			mdss_samsung_send_cmd(ctrl, PANEL_BRIGHT_CTRL);

		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].blic_dimming_cmds[vdd->panel_revision].cmds) ) {
			if(vdd->bl_level == 0)
				vdd->dtsi_data[ndx].blic_dimming_cmds->cmds->payload[1] = 0x24;
			else
				vdd->dtsi_data[ndx].blic_dimming_cmds->cmds->payload[1] = 0x2C;

			mdss_samsung_send_cmd(ctrl, PANEL_BLIC_DIMMING);
		}

			LCD_INFO("DSI%d level : %d  candela : %dCD hbm : %d\n",
				ndx, level, vdd->candela_level,
				vdd->display_ststus_dsi[ndx].hbm_mode);
		} else
			LCD_INFO("DSDI%d single_transmission_fail error\n", ndx);
	} else
		LCD_INFO("DSI%d level : %d skip\n", ndx, vdd->bl_level);

		if (vdd->auto_brightness >= HBM_MODE &&	vdd->bl_level == 255 
			&&	!vdd->dtsi_data[ndx].tft_common_support && vdd->support_mdnie_lite)
				update_dsi_tcon_mdnie_register(vdd);

	return cmd_cnt;
}
void mdss_samsung_brightness_tft_pwm(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_panel_info *pinfo;

	vdd = check_valid_ctrl(ctrl);
	if (vdd == NULL) {
		LCD_ERR("no PWM\n");
		return;
	}

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->blank_state == MDSS_PANEL_BLANK_BLANK)
		return;

	vdd->bl_level = level;

	if (vdd->panel_func.samsung_brightness_tft_pwm)
		vdd->panel_func.samsung_brightness_tft_pwm(ctrl,level);
}
void mdss_tft_autobrightness_cabc_update(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return;
	}
	LCD_INFO("\n");

	switch(vdd->auto_brightness)
	{
	case 0: mdss_samsung_cabc_update();
			break;
	case 1:
	case 2:
	case 3:
	case 4:
			mdss_samsung_send_cmd(ctrl, PANEL_CABC_ON);
			break;
	case 5:
	case 6:
			mdss_samsung_send_cmd(ctrl, PANEL_CABC_OFF);
			break;
	}
}
/*************************************************************
*
*		OSC TE FITTING RELATED FUNCTION BELOW.
*
**************************************************************/
static void mdss_samsung_event_osc_te_fitting(struct mdss_panel_data *pdata, int event, void *arg)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	struct osc_te_fitting_info *te_info = NULL;
	struct mdss_mdp_ctl *ctl = NULL;
	int ret, i, lut_count;

	if (IS_ERR_OR_NULL(pdata)) {
		LCD_ERR("Invalid pdata : 0x%zx\n", (size_t)pdata);
		return;
	}

	vdd = (struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	ctl = arg;

	if (IS_ERR_OR_NULL(ctrl) ||	IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(ctl)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx ctl : 0x%zx\n", (size_t)ctrl, (size_t)vdd, (size_t)ctl);
		return;
	}

	te_info = &vdd->te_fitting_info;

	if (IS_ERR_OR_NULL(te_info)) {
		LCD_ERR("Invalid te data : 0x%zx\n", (size_t)te_info);
		return;
	}

	if (pdata->panel_info.cont_splash_enabled) {
		LCD_ERR("cont splash enabled\n");
		return;
	}

	if (!ctl->vsync_handler.enabled) {
		LCD_DEBUG("vsync handler does not enabled yet\n");
		return;
	}

	te_info->status |= TE_FITTING_DONE;

	LCD_DEBUG("++\n");

	if (!(te_info->status & TE_FITTING_REQUEST_IRQ)) {
		te_info->status |= TE_FITTING_REQUEST_IRQ;

		ret = request_threaded_irq(
				gpio_to_irq(ctrl->disp_te_gpio),
				samsung_te_check_handler,
				NULL,
				IRQF_TRIGGER_FALLING,
				"VSYNC_GPIO",
				(void *)ctrl);
		if (ret)
			LCD_ERR("Failed to request_irq, ret=%d\n", ret);
		else
			disable_irq(gpio_to_irq(ctrl->disp_te_gpio));
		te_info->te_time =
			kzalloc(sizeof(long long) * te_info->sampling_rate, GFP_KERNEL);
		INIT_WORK(&te_info->work, samsung_te_check_done_work);
	}

	for (lut_count = 0; lut_count < OSC_TE_FITTING_LUT_MAX; lut_count++) {
		init_completion(&te_info->te_check_comp);
		te_info->status |= TE_CHECK_ENABLE;
		te_info->te_duration = 0;

		LCD_DEBUG("osc_te_fitting _irq : %d\n", gpio_to_irq(ctrl->disp_te_gpio));

		enable_irq(gpio_to_irq(ctrl->disp_te_gpio));
		ret = wait_for_completion_timeout(
				&te_info->te_check_comp, 1000);

		if (ret <= 0)
			LCD_ERR("timeout\n");

		for (i = 0; i < te_info->sampling_rate; i++) {
			te_info->te_duration +=
				(i != 0 ? (te_info->te_time[i] - te_info->te_time[i-1]) : 0);
			LCD_DEBUG("vsync time : %lld, sum : %lld\n",
					te_info->te_time[i], te_info->te_duration);
		}
		do_div(te_info->te_duration, te_info->sampling_rate - 1);
		LCD_INFO("ave vsync time : %lld\n", te_info->te_duration);
		te_info->status &= ~TE_CHECK_ENABLE;

		if (vdd->panel_func.samsung_osc_te_fitting)
			ret = vdd->panel_func.samsung_osc_te_fitting(ctrl);

		if (!ret)
			mdss_samsung_send_cmd(ctrl, PANEL_OSC_TE_FITTING);
		else
			break;
	}
	LCD_DEBUG("--\n");
}

static void samsung_te_check_done_work(struct work_struct *work)
{
	struct osc_te_fitting_info *te_info = NULL;

	te_info = container_of(work, struct osc_te_fitting_info, work);

	if (IS_ERR_OR_NULL(te_info)) {
		LCD_ERR("Invalid TE tuning data\n");
		return;
	}

	complete_all(&te_info->te_check_comp);
}

static irqreturn_t samsung_te_check_handler(int irq, void *handle)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct osc_te_fitting_info *te_info = NULL;
	static bool skip_first_te = true;
	static u8 count;

	if (skip_first_te) {
		skip_first_te = false;
		goto end;
	}

	if (IS_ERR_OR_NULL(handle)) {
		LCD_ERR("handle is null\n");
		goto end;
	}

	ctrl = (struct mdss_dsi_ctrl_pdata *)handle;

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	te_info = &vdd->te_fitting_info;

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return -EINVAL;
	}

	if (!(te_info->status & TE_CHECK_ENABLE))
		goto end;

	if (count < te_info->sampling_rate) {
		te_info->te_time[count++] =
			ktime_to_us(ktime_get());
	} else {
		disable_irq_nosync(gpio_to_irq(ctrl->disp_te_gpio));
		schedule_work(&te_info->work);
		skip_first_te = true;
		count = 0;
	}

end:
	return IRQ_HANDLED;
}

/*************************************************************
*
*		LDI FPS RELATED FUNCTION BELOW.
*
**************************************************************/
int ldi_fps(unsigned int input_fps)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = samsung_get_vdd();

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return 0;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (IS_ERR_OR_NULL(ctrl)) {
		LCD_ERR("ctrl is error");
		return 0;
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_change_ldi_fps))
		rc = vdd->panel_func.samsung_change_ldi_fps(ctrl, input_fps);

	if (rc)
		mdss_samsung_send_cmd(ctrl, PANEL_LDI_FPS_CHANGE);

	return rc;
}
EXPORT_SYMBOL(ldi_fps);

/*************************************************************
*
*		HMT RELATED FUNCTION BELOW.
*
**************************************************************/
static int get_candela_value_hmt(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].bkl[vdd->hmt_stat.hmt_bl_level];

	return vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].lux_tab[index];
}

static int get_cmd_idx_hmt(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].bkl[vdd->hmt_stat.hmt_bl_level];

	return vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].cmd_idx[index];
}

int mdss_samsung_hmt_brightenss_packet_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int cmd_cnt = 0;
	int level_key = 0;
	struct dsi_cmd_desc *packet = NULL;
	struct dsi_panel_cmds *tx_cmd = NULL;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	LCD_INFO("++\n");

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	/* init packet */
	packet = &vdd->brightness[ctrl->ndx].brightness_packet_dsi[0];

	if (vdd->smart_dimming_hmt_loaded_dsi[ctrl->ndx]) {
		/* aid/aor B2 */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_aid_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_aid_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* elvss B6 */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* vint F4 */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_vint_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_vint_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* gamma CA */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_gamma_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_gamma_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	LCD_INFO("--\n");

	return cmd_cnt;
}

int mdss_samsung_brightness_dcs_hmt(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct samsung_display_driver_data *vdd = NULL;
	int cmd_cnt;
	int ret = 0;
	int i;
	int candela_map_tab_size;

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->hmt_stat.hmt_bl_level = level;

	LCD_ERR("[HMT] hmt_bl_level(%d)\n", vdd->hmt_stat.hmt_bl_level);

	if ( level < 0 || level > 255 ) {
		LCD_ERR("[HMT] hmt_bl_level(%d) is out of range! set to 150cd \n", level);
		vdd->hmt_stat.cmd_idx_hmt = 28;
		vdd->hmt_stat.candela_level_hmt = 150;
	} else {
		vdd->hmt_stat.cmd_idx_hmt = get_cmd_idx_hmt(vdd, ctrl->ndx);
		vdd->hmt_stat.candela_level_hmt = get_candela_value_hmt(vdd, ctrl->ndx);
	}

	if (vdd->hmt_stat.hmt_low_persistence) {
		cmd_cnt = mdss_samsung_hmt_brightenss_packet_set(ctrl);
	} else {
		// If low persistence is off, reset the bl level(use normal aid).
		LCD_INFO("[HMT] [LOW PERSISTENCE OFF] - use normal aid for brightness\n");

		candela_map_tab_size =
			vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].lux_tab_size;

		for (i=0; i<candela_map_tab_size; i++) {
			if (vdd->hmt_stat.candela_level_hmt <=
				vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].lux_tab[i]) {
				LCD_INFO("hmt_cd_level (%d), normal_cd_level (%d) idx(%d)\n", vdd->hmt_stat.candela_level_hmt,
					vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].lux_tab[i], i);
				break;
			}
		}

		vdd->cmd_idx = i;
		vdd->candela_level = vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].lux_tab[i];

		LCD_INFO("[HMT] [LOW PERSISTENCE OFF] hmt_cd_level (%d) idx(%d) cd_level(%d)\n",
			vdd->hmt_stat.candela_level_hmt, vdd->cmd_idx, vdd->candela_level);

		cmd_cnt = mdss_samsung_normal_brightenss_packet_set(ctrl);
	}

	/* sending tx cmds */
	if (cmd_cnt) {
		/* setting tx cmds cmt */
		vdd->brightness[ctrl->ndx].brightness_packet_tx_cmds_dsi.cmd_cnt = cmd_cnt;

		/* generate single tx packet */
		ret = mdss_samsung_single_transmission_packet(vdd->brightness);

		if (ret) {
			mdss_samsung_send_cmd(ctrl, PANEL_BRIGHT_CTRL);

			LCD_INFO("DSI(%d) cmd_idx(%d), candela_level(%d), hmt_bl_level(%d)",
				ctrl->ndx, vdd->hmt_stat.cmd_idx_hmt, vdd->hmt_stat.candela_level_hmt, vdd->hmt_stat.hmt_bl_level);
		} else
			LCD_DEBUG("DSDI%d single_transmission_fail error\n", ctrl->ndx);
	} else
		LCD_INFO("DSI%d level : %d skip\n", ctrl->ndx, vdd->bl_level);

	return cmd_cnt;
}

/************************************************************

		PANEL DTSI PARSE FUNCTION.

		--- NEVER DELETE OR CHANGE ORDER ---
		---JUST ADD ITEMS AT LAST---

	"samsung,display_on_tx_cmds_rev%c"
	"samsung,display_off_tx_cmds_rev%c"
	"samsung,level1_key_enable_tx_cmds_rev%c"
	"samsung,level1_key_disable_tx_cmds_rev%c"
	"samsung,hsync_on_tx_cmds_rev%c"
	"samsung,level2_key_enable_tx_cmds_rev%c"
	"samsung,level2_key_disable_tx_cmds_rev%c"
	"samsung,smart_dimming_mtp_rx_cmds_rev%c"
	"samsung,manufacture_read_pre_tx_cmds_rev%c"
	"samsung,manufacture_id0_rx_cmds_rev%c"
	"samsung,manufacture_id1_rx_cmds_rev%c"
	"samsung,manufacture_id2_rx_cmds_rev%c"
	"samsung,manufacture_date_rx_cmds_rev%c"
	"samsung,ddi_id_rx_cmds_rev%c"
	"samsung,rddpm_rx_cmds_rev%c"
	"samsung,mtp_read_sysfs_rx_cmds_rev%c"
	"samsung,vint_tx_cmds_rev%c"
	"samsung,vint_map_table_rev%c"
	"samsung,acl_off_tx_cmds_rev%c"
	"samsung,acl_map_table_rev%c"
	"samsung,candela_map_table_rev%c"
	"samsung,acl_percent_tx_cmds_rev%c"
	"samsung,acl_on_tx_cmds_rev%c"
	"samsung,gamma_tx_cmds_rev%c"
	"samsung,elvss_rx_cmds_rev%c"
	"samsung,elvss_tx_cmds_rev%c"
	"samsung,aid_map_table_rev%c"
	"samsung,aid_tx_cmds_rev%c"
	"samsung,hbm_rx_cmds_rev%c"
	"samsung,hbm2_rx_cmds_rev%c"
	"samsung,hbm_gamma_tx_cmds_rev%c"
	"samsung,hbm_etc_tx_cmds_rev%c"
	"samsung,hbm_off_tx_cmds_rev%c"
	"samsung,mdnie_read_rx_cmds_rev%c"
	"samsung,ldi_debug0_rx_cmds_rev%c"
	"samsung,ldi_debug1_rx_cmds_rev%c"
	"samsung,ldi_debug2_rx_cmds_rev%c"
	"samsung,elvss_lowtemp_tx_cmds_rev%c"
	"samsung,elvss_lowtemp2_tx_cmds_rev%c"
	"samsung,smart_acl_elvss_tx_cmds_rev%c"
	"samsung,smart_acl_elvss_map_table_rev%c"
	"samsung,partial_display_on_tx_cmds_rev%c"
	"samsung,partial_display_off_tx_cmds_rev%c"
	"samsung,partial_display_column_row_tx_cmds_rev%c"
	"samsung,alpm_on_tx_cmds_rev%c"
	"samsung,alpm_off_tx_cmds_rev%c"
	"samsung,hmt_gamma_tx_cmds_rev%c"
	"samsung,hmt_elvss_tx_cmds_rev%c"
	"samsung,hmt_vint_tx_cmds_rev%c"
	"samsung,hmt_enable_tx_cmds_rev%c"
	"samsung,hmt_disable_tx_cmds_rev%c"
	"samsung,hmt_reverse_enable_tx_cmds_rev%c"
	"samsung,hmt_reverse_disable_tx_cmds_rev%c"
	"samsung,hmt_aid_tx_cmds_rev%c"
	"samsung,hmt_reverse_aid_map_table_rev%c"
	"samsung,hmt_150cd_rx_cmds_rev%c"
	"samsung,hmt_candela_map_table_rev%c"
	"samsung,ldi_fps_change_tx_cmds_rev%c"
	"samsung,ldi_fps_rx_cmds_rev%c"
	"samsung,tft_pwm_tx_cmds_rev%c"
	"samsung,scaled_level_map_table_rev%c"
	"samsung,packet_size_tx_cmds_rev%c"
	"samsung,reg_read_pos_tx_cmds_rev%c"
	"samsung,osc_te_fitting_tx_cmds_rev%c"
	"samsung,panel_ldi_vdd_read_cmds_rev%c"
	"samsung,panel_ldi_vddm_read_cmds_rev%c"
	"samsung,panel_ldi_vdd_offset_write_cmds_rev%c"
	"samsung,panel_ldi_vddm_offset_write_cmds_rev%c"
	"samsung,cabc_on_tx_cmds_rev%c"
	"samsung,cabc_off_tx_cmds_rev%c"
	"samsung,cabc_on_duty_tx_cmds_rev%c"
	"samsung,cabc_off_duty_tx_cmds_rev%c"
*************************************************************/
void mdss_samsung_panel_parse_dt_cmds(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rev_value = 'A';
	int panel_revision;
	char string[PARSE_STRING];
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	/* At this time ctrl->ndx is not set */
	int ndx = ctrl->panel_data.panel_info.pdest;
	static int parse_dt_cmds_cnt = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx\n", (size_t)ctrl, (size_t)vdd);
		return;
	} else
		LCD_INFO("DSI%d", ndx);

	if (vdd->support_hall_ic) {
		if (!parse_dt_cmds_cnt) {
			ndx = DSI_CTRL_0;
			parse_dt_cmds_cnt = 1;
		} else
			ndx = DSI_CTRL_1;
	}

	for (panel_revision = 0; panel_revision < SUPPORT_PANEL_REVISION; panel_revision++) {
		snprintf(string, PARSE_STRING, "samsung,display_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].display_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].display_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].display_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,display_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].display_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].display_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].display_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,level1_key_enable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].level1_key_enable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].level1_key_enable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].level1_key_enable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,level1_key_disable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].level1_key_disable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].level1_key_disable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].level1_key_disable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hsync_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hsync_on_tx_cmds [panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hsync_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hsync_on_tx_cmds [panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,level2_key_enable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].level2_key_enable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].level2_key_enable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].level2_key_enable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,level2_key_disable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].level2_key_disable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].level2_key_disable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].level2_key_disable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,smart_dimming_mtp_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].smart_dimming_mtp_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].smart_dimming_mtp_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].smart_dimming_mtp_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,manufacture_read_pre_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].manufacture_read_pre_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].manufacture_read_pre_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].manufacture_read_pre_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,manufacture_id0_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].manufacture_id0_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].manufacture_id0_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].manufacture_id0_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,manufacture_id1_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].manufacture_id1_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].manufacture_id1_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].manufacture_id1_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,manufacture_id2_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].manufacture_id2_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].manufacture_id2_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].manufacture_id2_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,manufacture_date_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].manufacture_date_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].manufacture_date_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].manufacture_date_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,ddi_id_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ddi_id_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ddi_id_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].ddi_id_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,cell_id_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].cell_id_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].cell_id_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].cell_id_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,rddpm_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].rddpm_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].rddpm_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].rddpm_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,mtp_read_sysfs_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].mtp_read_sysfs_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].mtp_read_sysfs_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].mtp_read_sysfs_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,vint_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].vint_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].vint_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].vint_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,vint_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_panel_table(np, &vdd->dtsi_data[ndx].vint_map_table[panel_revision], string) && panel_revision > 0) /* VINT TABLE */
			memcpy(&vdd->dtsi_data[ndx].vint_map_table[panel_revision], &vdd->dtsi_data[ndx].vint_map_table[panel_revision - 1], sizeof(struct cmd_map));

		snprintf(string, PARSE_STRING, "samsung,acl_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].acl_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].acl_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].acl_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,acl_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_panel_table(np, &vdd->dtsi_data[ndx].acl_map_table[panel_revision], string) && panel_revision > 0) /* ACL TABLE */
			memcpy(&vdd->dtsi_data[ndx].acl_map_table[panel_revision], &vdd->dtsi_data[ndx].acl_map_table[panel_revision - 1], sizeof(struct cmd_map));

		snprintf(string, PARSE_STRING, "samsung,candela_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_candella_lux_mapping_table(np, &vdd->dtsi_data[ndx].candela_map_table[panel_revision], string) && panel_revision > 0) /* CANDELA MAP TABLE */
					memcpy(&vdd->dtsi_data[ndx].candela_map_table[panel_revision], &vdd->dtsi_data[ndx].candela_map_table[panel_revision - 1], sizeof(struct candella_lux_map));

		snprintf(string, PARSE_STRING, "samsung,acl_percent_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].acl_percent_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].acl_percent_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].acl_percent_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,acl_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].acl_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].acl_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].acl_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,gamma_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].gamma_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].gamma_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].gamma_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,elvss_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].elvss_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].elvss_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].elvss_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,elvss_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].elvss_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].elvss_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].elvss_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,aid_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_panel_table(np, &vdd->dtsi_data[ndx].aid_map_table[panel_revision], string) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].aid_map_table[panel_revision], &vdd->dtsi_data[ndx].aid_map_table[panel_revision - 1], sizeof(struct cmd_map));/* AID TABLE */

		snprintf(string, PARSE_STRING, "samsung,aid_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].aid_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].aid_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].aid_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_HBM_RE */
		snprintf(string, PARSE_STRING, "samsung,hbm_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hbm_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hbm_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].hbm_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hbm2_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hbm2_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hbm2_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].hbm2_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hbm_gamma_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hbm_gamma_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hbm_gamma_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hbm_gamma_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hbm_etc_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hbm_etc_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hbm_etc_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hbm_etc_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hbm_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hbm_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hbm_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hbm_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_TCON_MDNIE_LITE */
		snprintf(string, PARSE_STRING, "samsung,mdnie_read_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].mdnie_read_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].mdnie_read_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].mdnie_read_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_DEBUG_LDI_STATUS */
		snprintf(string, PARSE_STRING, "samsung,ldi_debug0_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ldi_debug0_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ldi_debug0_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].ldi_debug0_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,ldi_debug1_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ldi_debug1_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ldi_debug1_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].ldi_debug1_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,ldi_debug2_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ldi_debug2_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ldi_debug2_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].ldi_debug2_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_TEMPERATURE_ELVSS */
		snprintf(string, PARSE_STRING, "samsung,elvss_lowtemp_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].elvss_lowtemp_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].elvss_lowtemp_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].elvss_lowtemp_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,elvss_lowtemp2_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].elvss_lowtemp2_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].elvss_lowtemp2_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].elvss_lowtemp2_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_SMART_ACL */
		snprintf(string, PARSE_STRING, "samsung,smart_acl_elvss_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].smart_acl_elvss_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].smart_acl_elvss_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].smart_acl_elvss_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,smart_acl_elvss_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_panel_table(np, &vdd->dtsi_data[ndx].smart_acl_elvss_map_table[panel_revision], string) && panel_revision > 0) /* TABLE */
			memcpy(&vdd->dtsi_data[ndx].smart_acl_elvss_map_table[panel_revision], &vdd->dtsi_data[ndx].smart_acl_elvss_map_table[panel_revision - 1], sizeof(struct cmd_map));

		/* CONFIG_PARTIAL_UPDATE */
		snprintf(string, PARSE_STRING, "samsung,partial_display_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].partial_display_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].partial_display_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].partial_display_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,partial_display_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].partial_display_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].partial_display_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].partial_display_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,partial_display_column_row_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].partial_display_column_row_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].partial_display_column_row_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].partial_display_column_row_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_ALPM_MODE */
		snprintf(string, PARSE_STRING, "samsung,alpm_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].alpm_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].alpm_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].alpm_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,alpm_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].alpm_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].alpm_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].alpm_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* HMT */
		snprintf(string, PARSE_STRING, "samsung,hmt_gamma_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_gamma_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_gamma_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_gamma_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_elvss_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_elvss_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_elvss_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_elvss_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_vint_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_vint_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_vint_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_vint_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_enable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_enable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_enable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_enable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_disable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_disable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_disable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_disable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_reverse_enable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_reverse_enable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_reverse_enable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_reverse_enable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_reverse_disable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_reverse_disable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_reverse_disable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_reverse_disable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_aid_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_aid_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_aid_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_aid_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_reverse_aid_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_panel_table(np, &vdd->dtsi_data[ndx].hmt_reverse_aid_map_table[panel_revision], string) && panel_revision > 0) /* TABLE */
			memcpy(&vdd->dtsi_data[ndx].hmt_reverse_aid_map_table[panel_revision], &vdd->dtsi_data[ndx].hmt_reverse_aid_map_table[panel_revision - 1], sizeof(struct cmd_map));
		snprintf(string, PARSE_STRING, "samsung,hmt_150cd_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_150cd_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_150cd_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_150cd_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));


		snprintf(string, PARSE_STRING, "samsung,hmt_candela_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_candella_lux_mapping_table(np, &vdd->dtsi_data[ndx].hmt_candela_map_table[panel_revision], string) && panel_revision > 0) /* TABLE */
			memcpy(&vdd->dtsi_data[ndx].hmt_candela_map_table[panel_revision], &vdd->dtsi_data[ndx].hmt_candela_map_table[panel_revision - 1], sizeof(struct candella_lux_map));

		/* CONFIG_FPS_CHANGE */
		snprintf(string, PARSE_STRING, "samsung,ldi_fps_change_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ldi_fps_change_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ldi_fps_change_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].ldi_fps_change_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,ldi_fps_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ldi_fps_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ldi_fps_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].ldi_fps_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* TFT PWM CONTROL */
		snprintf(string, PARSE_STRING, "samsung,tft_pwm_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].tft_pwm_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].tft_pwm_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].tft_pwm_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,blic_dimming_cmds_rev%c", panel_revision + rev_value);
			if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].blic_dimming_cmds[panel_revision], string, NULL) && panel_revision > 0)
				memcpy(&vdd->dtsi_data[ndx].blic_dimming_cmds[panel_revision], &vdd->dtsi_data[ndx].blic_dimming_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,scaled_level_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_candella_lux_mapping_table(np, &vdd->dtsi_data[ndx].scaled_level_map_table[panel_revision], string) && panel_revision > 0) /* SCALED LEVEL MAP TABLE */
			memcpy(&vdd->dtsi_data[ndx].scaled_level_map_table[panel_revision], &vdd->dtsi_data[ndx].scaled_level_map_table[panel_revision - 1], sizeof(struct candella_lux_map));

		snprintf(string, PARSE_STRING, "samsung,hbm_candela_map_table_rev%c", panel_revision + rev_value);
				if (mdss_samsung_parse_hbm_candella_lux_mapping_table(np, &vdd->dtsi_data[ndx].hbm_candela_map_table[panel_revision], string) && panel_revision > 0) /* hbm candella LEVEL MAP TABLE */
					memcpy(&vdd->dtsi_data[ndx].hbm_candela_map_table[panel_revision], &vdd->dtsi_data[ndx].hbm_candela_map_table[panel_revision - 1], sizeof(struct hbm_candella_lux_map));

		/* Additional Command */
		snprintf(string, PARSE_STRING, "samsung,packet_size_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].packet_size_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].packet_size_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].packet_size_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,reg_read_pos_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,osc_te_fitting_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].osc_te_fitting_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].osc_te_fitting_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].osc_te_fitting_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));
		/* samsung,avc_on */
		snprintf(string, PARSE_STRING, "samsung,avc_on_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].avc_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].avc_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].avc_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* VDDM OFFSET */
		snprintf(string, PARSE_STRING, "samsung,panel_ldi_vdd_read_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].read_vdd_ref_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].read_vdd_ref_cmds[panel_revision], &vdd->dtsi_data[ndx].read_vdd_ref_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,panel_ldi_vddm_read_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].read_vddm_ref_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].read_vddm_ref_cmds[panel_revision], &vdd->dtsi_data[ndx].read_vddm_ref_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,panel_ldi_vdd_offset_write_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].write_vdd_offset_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].write_vdd_offset_cmds[panel_revision], &vdd->dtsi_data[ndx].write_vdd_offset_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,panel_ldi_vddm_offset_write_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].write_vddm_offset_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].write_vddm_offset_cmds[panel_revision], &vdd->dtsi_data[ndx].write_vddm_offset_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* TFT CABC CONTROL */
		snprintf(string, PARSE_STRING, "samsung,cabc_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].cabc_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].cabc_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].cabc_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,cabc_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].cabc_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].cabc_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].cabc_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,cabc_on_duty_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].cabc_on_duty_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].cabc_on_duty_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].cabc_on_duty_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,cabc_off_duty_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].cabc_off_duty_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].cabc_off_duty_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].cabc_off_duty_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));
	}
}

/*
 * This will use to enable/disable or check the status of ALPM
 * * Description for STATUS_OR_EVENT_FLAG *
 *	1) ALPM_MODE_ON
 *	2) NORMAL_MODE_ON
 *		-> Set by user using sysfs(/sys/class/lcd/panel/alpm)
 *			The value will save to current_status
 *	3) CHECK_CURRENT_STATUS
 *		-> Check current status
 *			that will return current status like ALPM_MODE_ON, NORMAL_MODE_ON or MODE_OFF
 *	4) CHECK_PREVIOUS_STATUS
 *		-> Check previous status that will return previous status like
 *			 ALPM_MODE_ON, NORMAL_MODE_ON or MODE_OFF
 *	5) STORE_CURRENT_STATUS
 *		-> Store current status to previous status because that will use
 *			for next turn on sequence
 *	6) CLEAR_MODE_STATUS
 *		-> Clear current and previous status as MODE_OFF status that can use with
 *	* Usage *
 *		Call function "mdss_dsi_panel_alpm_status_func(STATUS_FLAG)"
 */
u8 alpm_status_func(u8 flag)
{
	static u8 current_status = 0;
	static u8 previous_status = 0;
	u8 ret = 0;

	if(!vdd_data.support_alpm)
		return 0;

	switch (flag) {
		case ALPM_MODE_ON:
			current_status = ALPM_MODE_ON;
			break;
		case NORMAL_MODE_ON:
			/*current_status = NORMAL_MODE_ON;*/
			break;
		case MODE_OFF:
			current_status = MODE_OFF;
			break;
		case CHECK_CURRENT_STATUS:
			ret = current_status;
			break;
		case CHECK_PREVIOUS_STATUS:
			ret = previous_status;
			break;
		case STORE_CURRENT_STATUS:
			previous_status = current_status;
			break;
		case CLEAR_MODE_STATUS:
			previous_status = 0;
			current_status = 0;
			break;
		default:
			break;
	}

	LCD_DEBUG("[ALPM_DEBUG] current_status : %d, previous_status : %d, ret : %d\n",\
				 current_status, previous_status, ret);

	return ret;
}

void mdss_samsung_panel_pbaboot_config(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_debug_data *mdd =
		(struct mdss_debug_data *)((mdss_mdp_get_mdata())->debug_inf.debug_data);
	struct samsung_display_driver_data *vdd = NULL;
	bool need_to_force_vidoe_mode = false;

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(mdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx, mdd : 9x%zx\n", (size_t)ctrl, (size_t)mdd);
		return;
	}

	pinfo = &ctrl->panel_data.panel_info;
	vdd = check_valid_ctrl(ctrl);

	if (vdd->support_hall_ic) {
		if (!get_lcd_attached("GET") && !get_lcd_attached_secondary("GET"))
			need_to_force_vidoe_mode = true;
	} else {
		if (!get_lcd_attached("GET"))
			need_to_force_vidoe_mode = true;
	}

	/* Support PBA boot without lcd */
	if (need_to_force_vidoe_mode &&
			!IS_ERR_OR_NULL(pinfo) &&
			!IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("force VIDEO_MODE : %d\n", ctrl->ndx);
		pinfo->type = MIPI_VIDEO_PANEL;
		pinfo->mipi.mode = DSI_VIDEO_MODE;
		pinfo->mipi.traffic_mode = DSI_BURST_MODE;
		pinfo->mipi.bllp_power_stop = true;
		pinfo->mipi.te_sel = 0;
		pinfo->mipi.vsync_enable = 0;
		pinfo->mipi.hw_vsync_mode = 0;
		pinfo->mipi.force_clk_lane_hs = true;
		pinfo->mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;

		pinfo->cont_splash_enabled = false;
		pinfo->mipi.lp11_init = false;

		pinfo->esd_check_enabled = false;
		ctrl->on_cmds.link_state = DSI_LP_MODE;
		ctrl->off_cmds.link_state = DSI_LP_MODE;

		/* To avoid underrun panic*/
		mdd->logd.xlog_enable = 0;
		vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting = false;
	}
}

void mdss_samsung_panel_parse_dt_esd(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;
	const char *data;
	struct samsung_display_driver_data *vdd = NULL;

	if (!ctrl) {
		LCD_ERR("ctrl is null\n");
		return;
	}

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return;
	}

	vdd->esd_recovery.esd_gpio = of_get_named_gpio(np, "qcom,esd_irq_gpio", 0);

	if (gpio_is_valid(vdd->esd_recovery.esd_gpio)) {
		LCD_INFO("esd gpio : %d, irq : %d\n",
				vdd->esd_recovery.esd_gpio,
				gpio_to_irq(vdd->esd_recovery.esd_gpio));
	}

	rc = of_property_read_string(np, "qcom,mdss-dsi-panel-status-check-mode", &data);
	if (!rc) {
		if (!strcmp(data, "reg_read_irq")) {
			ctrl->status_mode = ESD_REG_IRQ;
			ctrl->status_cmds_rlen = 0;
			ctrl->check_read_status = mdss_dsi_esd_irq_status;
		}
	}

	rc = of_property_read_string(np, "qcom,mdss-dsi-panel-status-irq-trigger", &data);
	if (!rc) {
		vdd->esd_recovery.irqflags = IRQF_ONESHOT;

		if (!strcmp(data, "rising"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_RISING;
		else if (!strcmp(data, "falling"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_FALLING;
		else if (!strcmp(data, "high"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_HIGH;
		else if (!strcmp(data, "low"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_LOW;
	}
}

void mdss_samsung_panel_parse_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc, i;
	u32 tmp[2];
	char panel_extra_power_gpio[] = "samsung,panel-extra-power-gpio1";
	char backlight_tft_gpio[] = "samsung,panel-backlight-tft-gpio1";
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	rc = of_property_read_u32(np, "samsung,support_panel_max", tmp);
	vdd->support_panel_max = !rc ? tmp[0] : 1;


	/* Set LP11 init flag */
	vdd->dtsi_data[ctrl->ndx].samsung_lp11_init = of_property_read_bool(np, "samsung,dsi-lp11-init");

	LCD_ERR("LP11 init %s\n", vdd->dtsi_data[ctrl->ndx].samsung_lp11_init ? "enabled" : "disabled");

	vdd->dtsi_data[ctrl->ndx].samsung_change_acl_by_brightness = of_property_read_bool(np, "samsung,samsung_change_acl_by_brightness");

	LCD_INFO("Change ACL by brightness mode %s\n",
			vdd->dtsi_data[ctrl->ndx].samsung_change_acl_by_brightness ? "enabled" : "disabled");

	rc = of_property_read_u32(np, "samsung,mdss-power-on-reset-delay-us", tmp);
	vdd->dtsi_data[ctrl->ndx].samsung_power_on_reset_delay = (!rc ? tmp[0] : 0);

	rc = of_property_read_u32(np, "samsung,mdss-dsi-off-reset-delay-us", tmp);
	vdd->dtsi_data[ctrl->ndx].samsung_dsi_off_reset_delay = (!rc ? tmp[0] : 0);

	/* Set esc clk 128M */
	vdd->dtsi_data[ctrl->ndx].samsung_esc_clk_128M = of_property_read_bool(np, "samsung,esc-clk-128M");
	LCD_ERR("ESC CLK 128M %s\n", vdd->dtsi_data[ctrl->ndx].samsung_esc_clk_128M ? "enabled" : "disabled");

	vdd->support_alpm  = of_property_read_bool(np, "qcom,mdss-dsi-alpm-enable");
	LCD_ERR("alpm enable %s\n", vdd->dtsi_data[ctrl->ndx].samsung_alpm_enable ? "enabled" : "disabled");

	/* Set HALL IC */
	vdd->support_hall_ic  = of_property_read_bool(np, "samsung,mdss_dsi_hall_ic_enable");
	LCD_ERR("hall_ic %s\n", vdd->support_hall_ic ? "enabled" : "disabled");

	/*Set OSC TE fitting flag */
	vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting =
		of_property_read_bool(np, "samsung,osc-te-fitting-enable");

	if (vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting) {
		rc = of_property_read_u32_array(np, "samsung,osc-te-fitting-cmd-index", tmp, 2);

		if (!rc) {
			vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting_cmd_index[0] =
				tmp[0];
			vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting_cmd_index[1] =
				tmp[1];
		}

		rc = of_property_read_u32(np, "samsung,osc-te-fitting-sampling-rate", tmp);

		vdd->te_fitting_info.sampling_rate = !rc ? tmp[0] : 2;

	}

	LCD_INFO("OSC TE fitting %s\n",
		vdd->dtsi_data[0].samsung_osc_te_fitting ? "enabled" : "disabled");

	/* Set HMT flag */
	vdd->dtsi_data[0].hmt_enabled = of_property_read_bool(np, "samsung,hmt_enabled");
	if (vdd->dtsi_data[0].hmt_enabled)
		for (i = 1; i < vdd->support_panel_max; i++)
			vdd->dtsi_data[i].hmt_enabled = true;

	LCD_INFO("hmt %s\n",
		vdd->dtsi_data[0].hmt_enabled ? "enabled" : "disabled");

	/* Set TFT flag */
	vdd->mdnie_tuning_enable_tft = of_property_read_bool(np,
				"samsung,mdnie-tuning-enable-tft");
	vdd->dtsi_data[ctrl->ndx].tft_common_support  = of_property_read_bool(np,
		"samsung,tft-common-support");

	LCD_INFO("tft_common_support %s\n",
	vdd->dtsi_data[ctrl->ndx].tft_common_support ? "enabled" : "disabled");

	vdd->dtsi_data[ctrl->ndx].tft_module_name = of_get_property(np,
		"samsung,tft-module-name",NULL);  //for tft tablet

	vdd->dtsi_data[ctrl->ndx].panel_vendor = of_get_property(np,
		"samsung,panel-vendor",NULL);

	vdd->dtsi_data[ctrl->ndx].backlight_gpio_config = of_property_read_bool(np,
		"samsung,backlight-gpio-config");

	LCD_INFO("backlight_gpio_config %s\n",
	vdd->dtsi_data[ctrl->ndx].backlight_gpio_config ? "enabled" : "disabled");

	/* Set extra power gpio */
	for (i = 0; i < MAX_EXTRA_POWER_GPIO; i++) {
		panel_extra_power_gpio[strlen(panel_extra_power_gpio) - 1] = '1' + i;
		vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i] =
				 of_get_named_gpio(np,
						panel_extra_power_gpio, 0);
		if (!gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]))
			LCD_ERR("%d, panel_extra_power gpio%d not specified\n", __LINE__, i+1);
		else
			LCD_ERR("extra gpio num : %d\n", vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]);
	}

	/* Set tft backlight gpio */
	for (i = 0; i < MAX_BACKLIGHT_TFT_GPIO; i++) {
		backlight_tft_gpio[strlen(backlight_tft_gpio) - 1] = '1' + i;
		vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i] =
				 of_get_named_gpio(np,
						backlight_tft_gpio, 0);
		if (!gpio_is_valid(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i]))
			LCD_ERR("%d, backlight_tft_gpio gpio%d not specified\n", __LINE__, i+1);
		else
			LCD_ERR("tft gpio num : %d\n", vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i]);
	}

	/* Set Mdnie lite HBM_CE_TEXT_MDNIE mode used */
	vdd->dtsi_data[ctrl->ndx].hbm_ce_text_mode_support  = of_property_read_bool(np, "samsung,hbm_ce_text_mode_support");
	
	/* Set Flag for outdoor mode support */
	vdd->dtsi_data[ctrl->ndx].outdoor_mode_support  = of_property_read_bool(np, "samsung,outdoor_mode_support");

	/* Set Backlight IC discharge time */
	rc = of_property_read_u32(np, "samsung,blic-discharging-delay-us", tmp);
	vdd->dtsi_data[ctrl->ndx].blic_discharging_delay_tft = (!rc ? tmp[0] : 6);

	/* Set cabc delay time */
	rc = of_property_read_u32(np, "samsung,cabc-delay-us", tmp);
	vdd->dtsi_data[ctrl->ndx].cabc_delay = (!rc ? tmp[0] : 6);

	mdss_samsung_panel_parse_dt_cmds(np, ctrl);
	if (vdd->support_hall_ic) {
		vdd->hall_ic_notifier_display.priority = 0; //Tsp is 1, Touch key is 2
		vdd->hall_ic_notifier_display.notifier_call = samsung_display_hall_ic_status;
		//hall_ic_register_notify(&vdd->hall_ic_notifier_display);

		mdss_samsung_panel_parse_dt_cmds(np, ctrl);
		vdd->mdnie_tune_state_dsi[DISPLAY_2] = init_dsi_tcon_mdnie_class(DISPLAY_2, vdd);

		vdd->dtsi_data[DISPLAY_2].samsung_change_acl_by_brightness =
			vdd->dtsi_data[DISPLAY_1].samsung_change_acl_by_brightness;
	}

	mdss_samsung_panel_parse_dt_esd(np, ctrl);
	mdss_samsung_panel_pbaboot_config(np, ctrl);
}


/************************************************************
*
*		SYSFS RELATED FUNCTION
*
**************************************************************/
#if defined(CONFIG_LCD_CLASS_DEVICE)
#if 0
/* 
 * Do not use below code but only for Image Quality Debug in Developing Precess.
 * Do not comment in this code Because there are contained vulnerability.
 */
static char char_to_dec(char data1, char data2)
{
	char dec;

	dec = 0;

	if (data1 >= 'a') {
		data1 -= 'a';
		data1 += 10;
	} else if (data1 >= 'A') {
		data1 -= 'A';
		data1 += 10;
	} else
		data1 -= '0';

	dec = data1 << 4;

	if (data2 >= 'a') {
		data2 -= 'a';
		data2 += 10;
	} else if (data2 >= 'A') {
		data2 -= 'A';
		data2 += 10;
	} else
		data2 -= '0';

	dec |= data2;

	return dec;
}

static void sending_tune_cmd(struct device *dev, char *src, int len)
{
	int data_pos;
	int cmd_step;
	int cmd_pos;

	char *mdnie_tuning1;
	char *mdnie_tuning2;
	char *mdnie_tuning3;
	char *mdnie_tuning4;
	char *mdnie_tuning5;
	char *mdnie_tuning6;

	struct dsi_cmd_desc *mdnie_tune_cmd;

	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error\n");
		return;
	}

	if (!vdd->mdnie_tune_size1 || !vdd->mdnie_tune_size2) {
		LCD_ERR("mdnie_tune_size is zero 1(%d) 2(%d)\n", vdd->mdnie_tune_size1, vdd->mdnie_tune_size2);
		return;
	}

	if(vdd->mdnie_tuning_enable_tft) {
		mdnie_tune_cmd = kzalloc(7 * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
		mdnie_tuning1 = kzalloc(sizeof(char) * vdd->mdnie_tune_size1, GFP_KERNEL);
		mdnie_tuning2 = kzalloc(sizeof(char) * vdd->mdnie_tune_size2, GFP_KERNEL);
		mdnie_tuning3 = kzalloc(sizeof(char) * vdd->mdnie_tune_size3, GFP_KERNEL);
		mdnie_tuning4 = kzalloc(sizeof(char) * vdd->mdnie_tune_size4, GFP_KERNEL);
		mdnie_tuning5 = kzalloc(sizeof(char) * vdd->mdnie_tune_size5, GFP_KERNEL);
		mdnie_tuning6 = kzalloc(sizeof(char) * vdd->mdnie_tune_size6, GFP_KERNEL);

	} else {
	        mdnie_tune_cmd = kzalloc(2 * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
		mdnie_tuning1 = kzalloc(sizeof(char) * vdd->mdnie_tune_size1, GFP_KERNEL);
		mdnie_tuning2 = kzalloc(sizeof(char) * vdd->mdnie_tune_size2, GFP_KERNEL);
	}

	cmd_step = 0;
	cmd_pos = 0;

	for (data_pos = 0; data_pos < len;) {
		if (*(src + data_pos) == '0') {
			if (*(src + data_pos + 1) == 'x') {
				if (!cmd_step)
					mdnie_tuning1[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if ((cmd_step == 1) && vdd->mdnie_tuning_enable_tft)
					mdnie_tuning2[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if(cmd_step == 2 && vdd->mdnie_tuning_enable_tft)
					mdnie_tuning3[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if(cmd_step == 3 && vdd->mdnie_tuning_enable_tft)
					mdnie_tuning4[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if(cmd_step == 4 && vdd->mdnie_tuning_enable_tft)
					mdnie_tuning5[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if(cmd_step == 5 && vdd->mdnie_tuning_enable_tft)
					mdnie_tuning6[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else
					mdnie_tuning2[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));

				data_pos += 3;
				cmd_pos++;

				if (cmd_pos == vdd->mdnie_tune_size1 && !cmd_step) {
					cmd_pos = 0;
					cmd_step = 1;
				} else if((cmd_pos ==  vdd->mdnie_tune_size2) && (cmd_step == 1)  && vdd->mdnie_tuning_enable_tft){
													   cmd_pos = 0;
													   cmd_step = 2;
				} else if((cmd_pos ==  vdd->mdnie_tune_size3) && (cmd_step == 2) && vdd->mdnie_tuning_enable_tft){
													   cmd_pos = 0;
													   cmd_step = 3;
				} else if((cmd_pos ==  vdd->mdnie_tune_size4) && (cmd_step == 3) && vdd->mdnie_tuning_enable_tft){
													   cmd_pos = 0;
													   cmd_step = 4;
				} else if((cmd_pos ==  vdd->mdnie_tune_size5) && (cmd_step == 4) && vdd->mdnie_tuning_enable_tft){
													   cmd_pos = 0;
													   cmd_step = 5;
				}
			} else
				data_pos++;
		} else {
			data_pos++;
		}
	}

	mdnie_tune_cmd[0].dchdr.dtype = DTYPE_DCS_LWRITE;
	mdnie_tune_cmd[0].dchdr.last = 1;
	mdnie_tune_cmd[0].dchdr.dlen = vdd->mdnie_tune_size1;
	mdnie_tune_cmd[0].payload = mdnie_tuning1;

	mdnie_tune_cmd[1].dchdr.dtype = DTYPE_DCS_LWRITE;
	mdnie_tune_cmd[1].dchdr.last = 1;
	mdnie_tune_cmd[1].dchdr.dlen = vdd->mdnie_tune_size2;
	mdnie_tune_cmd[1].payload = mdnie_tuning2;

	printk(KERN_ERR "\n");
	for (data_pos = 0; data_pos < vdd->mdnie_tune_size1 ; data_pos++)
		printk(KERN_ERR "0x%x \n", mdnie_tuning1[data_pos]);
	printk(KERN_ERR "\n");
	for (data_pos = 0; data_pos < vdd->mdnie_tune_size2 ; data_pos++)
		printk(KERN_ERR "0x%x \n", mdnie_tuning2[data_pos]);

	if(vdd->mdnie_tuning_enable_tft) {
		mdnie_tune_cmd[2].dchdr.dtype = DTYPE_DCS_LWRITE;
		mdnie_tune_cmd[2].dchdr.last = 1;
		mdnie_tune_cmd[2].dchdr.dlen = vdd->mdnie_tune_size3;
		mdnie_tune_cmd[2].payload = mdnie_tuning3 ;

		mdnie_tune_cmd[3].dchdr.dtype = DTYPE_DCS_LWRITE;
		mdnie_tune_cmd[3].dchdr.last = 1;
		mdnie_tune_cmd[3].dchdr.dlen = vdd->mdnie_tune_size4;
		mdnie_tune_cmd[3].payload = mdnie_tuning4;

		mdnie_tune_cmd[4].dchdr.dtype = DTYPE_DCS_LWRITE;
		mdnie_tune_cmd[4].dchdr.last = 1;
		mdnie_tune_cmd[4].dchdr.dlen = vdd->mdnie_tune_size5;
		mdnie_tune_cmd[4].payload = mdnie_tuning5;

		mdnie_tune_cmd[5].dchdr.dtype = DTYPE_DCS_LWRITE;
		mdnie_tune_cmd[5].dchdr.last = 1;
		mdnie_tune_cmd[5].dchdr.dlen = vdd->mdnie_tune_size6;
		mdnie_tune_cmd[5].payload = mdnie_tuning6;

		printk(KERN_ERR "\n");
		for (data_pos = 0; data_pos < vdd->mdnie_tune_size3 ; data_pos++)
			printk(KERN_ERR "0x%x ", mdnie_tuning3[data_pos]);
		printk(KERN_ERR "\n");
		for (data_pos = 0; data_pos < vdd->mdnie_tune_size4 ; data_pos++)
			printk(KERN_ERR "0x%x ", mdnie_tuning4[data_pos]);
		printk(KERN_ERR "\n");
		for (data_pos = 0; data_pos < vdd->mdnie_tune_size5 ; data_pos++)
			printk(KERN_ERR "0x%x ", mdnie_tuning5[data_pos]);
		printk(KERN_ERR "\n");
		for (data_pos = 0; data_pos < vdd->mdnie_tune_size6 ; data_pos++)
			printk(KERN_ERR "0x%x ", mdnie_tuning6[data_pos]);
		printk(KERN_ERR "\n");
	}

	if (IS_ERR_OR_NULL(vdd))
		LCD_ERR("vdd is error");
	else {
		if((vdd->ctrl_dsi[DSI_CTRL_0]->cmd_sync_wait_broadcast)
		   && (vdd->ctrl_dsi[DSI_CTRL_1]->cmd_sync_wait_trigger)){ /* Dual DSI & dsi 1 trigger */
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_LEVE1_KEY_ENABLE);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_LEVE2_KEY_ENABLE);

			/* Set default link_stats as DSI_HS_MODE for mdnie tune data */
			vdd->mdnie_tune_data[DSI_CTRL_1].mdnie_tune_packet_tx_cmds_dsi.link_state = DSI_HS_MODE;
			vdd->mdnie_tune_data[DSI_CTRL_1].mdnie_tune_packet_tx_cmds_dsi.cmds = mdnie_tune_cmd;
			vdd->mdnie_tune_data[DSI_CTRL_1].mdnie_tune_packet_tx_cmds_dsi.cmd_cnt = 2;
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_MDNIE_TUNE);

			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_LEVE1_KEY_DISABLE);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_LEVE2_KEY_DISABLE);
		} else { /* Single DSI, dsi 0 trigger */
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_LEVE1_KEY_ENABLE);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_LEVE2_KEY_ENABLE);

			/* Set default link_stats as DSI_HS_MODE for mdnie tune data */
			vdd->mdnie_tune_data[DSI_CTRL_0].mdnie_tune_packet_tx_cmds_dsi.link_state = DSI_HS_MODE;
			vdd->mdnie_tune_data[DSI_CTRL_0].mdnie_tune_packet_tx_cmds_dsi.cmds = mdnie_tune_cmd;
			vdd->mdnie_tune_data[DSI_CTRL_0].mdnie_tune_packet_tx_cmds_dsi.cmd_cnt = vdd->mdnie_tuning_enable_tft ? 6 : 2;
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_MDNIE_TUNE);

			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_LEVE1_KEY_DISABLE);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_LEVE2_KEY_DISABLE);
		}
	}

	if(vdd->mdnie_tuning_enable_tft) {
		kfree(mdnie_tune_cmd);
		kfree(mdnie_tuning1);
		kfree(mdnie_tuning2);
		kfree(mdnie_tuning3);
		kfree(mdnie_tuning4);
		kfree(mdnie_tuning5);
		kfree(mdnie_tuning6);

	} else {
		kfree(mdnie_tune_cmd);
		kfree(mdnie_tuning1);
		kfree(mdnie_tuning2);
	}
}

static void load_tuning_file(struct device *dev, char *filename)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs;

	LCD_INFO("called loading file name : [%s]\n",
	       filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_ERR "%s File open failed\n", __func__);
		goto err;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	LCD_INFO("Loading File Size : %ld(bytes)", l);

	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		LCD_INFO("Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		goto err;
	}
	pos = 0;
	memset(dp, 0, l);

	LCD_INFO("before vfs_read()\n");
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	LCD_INFO("after vfs_read()\n");

	if (ret != l) {
		LCD_INFO("vfs_read() filed ret : %d\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		goto err;
	}

	filp_close(filp, current->files);

	set_fs(fs);

	sending_tune_cmd(dev, dp, l);

	kfree(dp);
	
	return;
err:
	set_fs(fs);
}
#endif

static ssize_t tuning_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, MAX_FILE_NAME, "Tunned File Name : %s\n", tuning_file);
	return ret;
}

static ssize_t tuning_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t size)
{
/* 
 * Do not use below code but only for Image Quality Debug in Developing Precess.
 * Do not comment in this code Because there are contained vulnerability.
 */
/*
	char *pt;
	if (buf == NULL || strchr(buf, '.') || strchr(buf, '/'))
		return size;

	memset(tuning_file, 0, sizeof(tuning_file));
	snprintf(tuning_file, MAX_FILE_NAME, "%s%s", TUNING_FILE_PATH, buf);

	pt = tuning_file;

	while (*pt) {
		if (*pt == '\r' || *pt == '\n') {
			*pt = 0;
			break;
		}
		pt++;
	}

	LCD_INFO("%s\n", tuning_file);

	load_tuning_file(dev, tuning_file);
*/
	return size;
}

static DEVICE_ATTR(tuning, 0664, tuning_show, tuning_store);
#endif

static ssize_t mdss_samsung_disp_cell_id_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	static int string_size = 50;
	char temp[string_size];
	int *cell_id;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return strnlen(buf, string_size);
	}

	cell_id = &vdd->cell_id_dsi[display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0])][0];

	snprintf((char *)temp, sizeof(temp),
			"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		cell_id[0], cell_id[1], cell_id[2], cell_id[3], cell_id[4],
		cell_id[5], cell_id[6], cell_id[7], cell_id[8], cell_id[9],
		cell_id[10]);

	strlcat(buf, temp, string_size);

	LCD_INFO("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", cell_id[0], cell_id[1], cell_id[2], cell_id[3],
		cell_id[4], cell_id[5], cell_id[6], cell_id[7], cell_id[8],
		cell_id[9], cell_id[10]);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	static int string_size = 100;
	char temp[string_size];
	int ndx;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return strnlen(buf, string_size);
	}

	ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);

	if(vdd->dtsi_data[ndx].tft_common_support && vdd->dtsi_data[ndx].tft_module_name){
		if(vdd->dtsi_data[ndx].panel_vendor)
			snprintf(temp, 20, "%s_%s\n",vdd->dtsi_data[ndx].panel_vendor,vdd->dtsi_data[ndx].tft_module_name);
		else
			snprintf(temp, 20, "SDC_%s\n",vdd->dtsi_data[ndx].tft_module_name);
	} else if(vdd->manufacture_id_dsi[ndx]){
		if(vdd->dtsi_data[ndx].panel_vendor)
			snprintf(temp, 20, "%s_%06x\n",vdd->dtsi_data[ndx].panel_vendor,vdd->manufacture_id_dsi[ndx]);
		else
			snprintf(temp, 20, "SDC_%06x\n", vdd->manufacture_id_dsi[ndx]);
	} else {
		if (get_lcd_attached("GET")) {
			if(vdd->dtsi_data[ndx].panel_vendor)
				snprintf(temp, 20, "%s_%06x\n",vdd->dtsi_data[ndx].panel_vendor, get_lcd_attached("GET"));
			else
				snprintf(temp, 20, "SDC_%06x\n", get_lcd_attached("GET"));
		} else
			LCD_INFO("no manufacture id\n");
	}

	strlcat(buf, temp, string_size);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_windowtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 15;
	char temp[string_size];
	int id, id1, id2, id3;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return strnlen(buf, string_size);
	}

	id = vdd->manufacture_id_dsi[display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0])];

	id1 = (id & 0x00FF0000) >> 16;
	id2 = (id & 0x0000FF00) >> 8;
	id3 = id & 0xFF;

	if (id1 == 0 && id2 == 0 && id3 == 0) {
		if (get_lcd_attached("GET")) {
			id1 = (get_lcd_attached("GET") & 0x00FF0000) >> 16;
			id2 = (get_lcd_attached("GET") & 0x0000FF00) >> 8;
			id3 = get_lcd_attached("GET") & 0xFF;
		}
	}

	snprintf(temp, sizeof(temp), "%02x %02x %02x\n", id1, id2, id3);

	strlcat(buf, temp, string_size);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_manufacture_date_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 30;
	char temp[string_size];
	int date;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return strnlen(buf, string_size);
	}

	date = vdd->manufacture_date_dsi[display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0])];
	snprintf((char *)temp, sizeof(temp), "manufacture date : %d\n", date);

	strlcat(buf, temp, string_size);

	LCD_INFO("manufacture date : %d\n", date);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_manufacture_code_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 30;
	char temp[string_size];
	int *ddi_id;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return strnlen(buf, string_size);
	}

	ddi_id = &vdd->ddi_id_dsi[display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0])][0];

	snprintf((char *)temp, sizeof(temp), "%02x%02x%02x%02x%02x\n",
		ddi_id[0], ddi_id[1], ddi_id[2], ddi_id[3], ddi_id[4]);

	strlcat(buf, temp, string_size);

	LCD_INFO("%02x %02x %02x %02x %02x\n",
		ddi_id[0], ddi_id[1], ddi_id[2], ddi_id[3], ddi_id[4]);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_acl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return rc;
	}

	rc = snprintf((char *)buf, sizeof(vdd->acl_status), "%d\n", vdd->acl_status);

	LCD_INFO("acl status: %d\n", *buf);

	return rc;
}

static ssize_t mdss_samsung_disp_acl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int acl_set = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		LCD_ERR("vdd is error");
		return size;
	}

	if (sysfs_streq(buf, "1"))
		acl_set = true;
	else if (sysfs_streq(buf, "0"))
		acl_set = false;
	else
		LCD_INFO("Invalid argument!!");

	LCD_INFO("(%d)\n", acl_set);

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;
	mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	if (acl_set && !vdd->acl_status) {
		vdd->acl_status = acl_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else if (!acl_set && vdd->acl_status) {
		vdd->acl_status = acl_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else {
		vdd->acl_status = acl_set;
		LCD_INFO("skip acl update!! acl %d", vdd->acl_status);
	}

	mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	return size;
}

static ssize_t mdss_samsung_disp_siop_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return rc;
	}

	rc = snprintf((char *)buf, sizeof(vdd->siop_status), "%d\n", vdd->siop_status);

	LCD_INFO("siop status: %d\n", *buf);

	return rc;
}

static ssize_t mdss_samsung_disp_siop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int siop_set = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;


	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		LCD_ERR("vdd is error");
		return size;
	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	if (sysfs_streq(buf, "1"))
		siop_set = true;
	else if (sysfs_streq(buf, "0"))
		siop_set = false;
	else
		LCD_INFO("Invalid argument!!");

	LCD_INFO("(%d)\n", siop_set);

	mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	if (siop_set && !vdd->siop_status) {
		vdd->siop_status = siop_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else if (!siop_set && vdd->siop_status) {
		vdd->siop_status = siop_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else {
		vdd->siop_status = siop_set;
		LCD_INFO("skip siop update!! acl %d", vdd->acl_status);
	}

	mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	return size;
}

static ssize_t mdss_samsung_aid_log_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int loop = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return rc;
	}

	for (loop = 0; loop < vdd->support_panel_max; loop++) {
		if (vdd->smart_dimming_dsi[loop] && vdd->smart_dimming_dsi[loop]->print_aid_log)
			vdd->smart_dimming_dsi[loop]->print_aid_log(vdd->smart_dimming_dsi[loop]);
		else
			LCD_ERR("DSI%d smart dimming is not loaded\n", loop);
	}

	if (vdd->dtsi_data[0].hmt_enabled) {
		for (loop = 0; loop < vdd->support_panel_max; loop++) {
			if (vdd->smart_dimming_dsi_hmt[loop] && vdd->smart_dimming_dsi_hmt[loop]->print_aid_log)
				vdd->smart_dimming_dsi_hmt[loop]->print_aid_log(vdd->smart_dimming_dsi_hmt[loop]);
			else
				LCD_ERR("DSI%d smart dimming hmt is not loaded\n", loop);
		}
	}

	return rc;
}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static ssize_t mdss_samsung_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc =0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return rc;
	}

	rc = snprintf((char *)buf, sizeof(vdd->auto_brightness), "%d\n", vdd->auto_brightness);

	LCD_INFO("auto_brightness: %d\n", *buf);

	return rc;
}

static ssize_t mdss_samsung_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		LCD_ERR("vdd is error");
		return size;
	}
	if(!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].hbm_candela_map_table[vdd->panel_revision].cmd_idx))
		return size;

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	if (sysfs_streq(buf, "0"))
		vdd->auto_brightness = 0;
	else if (sysfs_streq(buf, "1"))
		vdd->auto_brightness = 1;
	else if (sysfs_streq(buf, "2"))
		vdd->auto_brightness = 2;
	else if (sysfs_streq(buf, "3"))
		vdd->auto_brightness = 3;
	else if (sysfs_streq(buf, "4"))
		vdd->auto_brightness = 4;
	else if (sysfs_streq(buf, "5"))
		vdd->auto_brightness = 5;
	else if (sysfs_streq(buf, "6")) // HBM mode
		vdd->auto_brightness = 6;
	else if (sysfs_streq(buf, "7"))
		vdd->auto_brightness = 7;
	else
		LCD_INFO("Invalid argument!!");

	LCD_INFO("(%d) \n", vdd->auto_brightness);

	if(!alpm_status_func(CHECK_PREVIOUS_STATUS)){
		if(!vdd->dtsi_data[DISPLAY_1].tft_common_support){
			mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
			pdata->set_backlight(pdata, vdd->bl_level);
			mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		}
		if(vdd->mdss_panel_tft_outdoormode_update)
			vdd->mdss_panel_tft_outdoormode_update(vdd->ctrl_dsi[DISPLAY_1]);
		else if(vdd->support_cabc)
			mdss_tft_autobrightness_cabc_update(vdd->ctrl_dsi[DISPLAY_1]);

		if (vdd->support_mdnie_lite)
			update_dsi_tcon_mdnie_register(vdd);
	} else
		LCD_ERR("[ALPM_DEBUG] ALPM is on. do not set brightness and mdnie..\n");

	return size;
}

static ssize_t mdss_samsung_weakness_hbm_comp_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc =0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return rc;
	}

	rc = snprintf((char *)buf, sizeof(vdd->weakness_hbm_comp), "%d\n", vdd->weakness_hbm_comp);

	LCD_INFO("weakness_hbm_comp: %c\n", *buf);

	return rc;
}

static ssize_t mdss_samsung_weakness_hbm_comp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		LCD_ERR("vdd is error");
		return size;
	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	if (sysfs_streq(buf, "0"))
		vdd->weakness_hbm_comp = 0;
	else if (sysfs_streq(buf, "1"))
		vdd->weakness_hbm_comp = 1;
	else if (sysfs_streq(buf, "2"))
		vdd->weakness_hbm_comp = 2;
	else if (sysfs_streq(buf, "3"))
		vdd->weakness_hbm_comp = 3;
	else
		LCD_INFO("Invalid argument!!");

	LCD_INFO("(%d) \n", vdd->weakness_hbm_comp);

	/* AMOLED Only */
	if(!vdd->dtsi_data[DISPLAY_1].tft_common_support){
		mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		pdata->set_backlight(pdata, vdd->bl_level);
		mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
	}

	return size;
}
#endif

static ssize_t mdss_samsung_read_mtp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int addr, len, start;
	char *read_buf = NULL;
	char read_size, read_startoffset;
	struct dsi_panel_cmds *rx_cmds;
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		LCD_ERR("vdd is error");
		return size;
	}
	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (IS_ERR_OR_NULL(ctrl)) {
		LCD_ERR("ctrl is error");
		return size;
	}

	sscanf(buf, "%x %d %x" , &addr, &len, &start);

	read_buf = kmalloc(len * sizeof(char), GFP_KERNEL);

	LCD_INFO("%x %d %x\n", addr, len, start);

	rx_cmds = &(vdd->dtsi_data[display_ndx_check(ctrl)].mtp_read_sysfs_rx_cmds[vdd->panel_revision]);

	rx_cmds->cmds[0].payload[0] =  addr;
	rx_cmds->cmds[0].payload[1] =  len;
	rx_cmds->cmds[0].payload[2] =  start;

	read_size = len;
	read_startoffset = start;

	rx_cmds->read_size =  &read_size;
	rx_cmds->read_startoffset =  &read_startoffset;


	LCD_INFO("%x %x %x %x %x %x %x %x %x\n",
		rx_cmds->cmds[0].dchdr.dtype,
		rx_cmds->cmds[0].dchdr.last,
		rx_cmds->cmds[0].dchdr.vc,
		rx_cmds->cmds[0].dchdr.ack,
		rx_cmds->cmds[0].dchdr.wait,
		rx_cmds->cmds[0].dchdr.dlen,
		rx_cmds->cmds[0].payload[0],
		rx_cmds->cmds[0].payload[1],
		rx_cmds->cmds[0].payload[2]);

	mdss_samsung_panel_data_read(ctrl, rx_cmds, read_buf, PANEL_LEVE1_KEY | PANEL_LEVE2_KEY);

	kfree(read_buf);

	return size;
}

static ssize_t mdss_samsung_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return rc;
	}

	rc = snprintf((char *)buf, 40,"-20, -19, -15, -14, 0, 1, 30, 40\n");

	LCD_INFO("temperature : %d", vdd->temperature);

	return rc;
}

static ssize_t mdss_samsung_temperature_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;
	int pre_temp = 0;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		LCD_ERR("vdd is error");
		return size;
	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;
	pre_temp = vdd->temperature;

	sscanf(buf, "%d" , &vdd->temperature);

	/* When temperature changed, hbm_mode must setted 0 for EA8061 hbm setting. */
	if(pre_temp != vdd->temperature && vdd->display_ststus_dsi[DISPLAY_1].hbm_mode == 1 )
		vdd->display_ststus_dsi[DISPLAY_1].hbm_mode = 0;

	mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
	pdata->set_backlight(pdata, vdd->bl_level);
	mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	LCD_INFO("temperature : %d", vdd->temperature);

	return size;
}

static ssize_t mdss_samsung_disp_partial_disp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	LCD_DEBUG("TDB");

	return 0;
}

static ssize_t mdss_samsung_disp_partial_disp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	LCD_DEBUG("TDB");

	return size;
}

static ssize_t mdss_samsung_alpm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
	(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_dsi_ctrl_pdata *ctrl;
	u8 current_status = 0;

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (!vdd->support_alpm)
		return rc;

	current_status = alpm_status_func(CHECK_CURRENT_STATUS);

	rc = snprintf((char *)buf, 30, "%d\n", current_status);
	LCD_INFO("[ALPM_DEBUG] current status : %d \n", (int)current_status);

	return rc;
}

static ssize_t mdss_samsung_alpm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int mode = 0;
	struct samsung_display_driver_data *vdd =
	(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;
	pinfo = &pdata->panel_info;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		LCD_ERR("vdd is error");
		return size;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (!vdd->support_alpm)
		return size;


	sscanf(buf, "%d" , &mode);
	LCD_INFO("[ALPM_DEBUG] mode : %d\n", mode);

	/*
	 * Possible mode status for Blank(0) or Unblank(1)
	 *	* Blank *
	 *		1) ALPM_MODE_ON
	 *			-> That will set during wakeup
	 *	* Unblank *
	 *		1) NORMAL_MODE_ON
	 *			-> That will send partial update commands
	 */
	alpm_status_func(mode ? ALPM_MODE_ON : MODE_OFF);
	if (mode == ALPM_MODE_ON) {
		/*
		 * This will work if the ALPM must be on or chagne partial area
		 * if that already in the status of unblank
		 */
		if (pinfo->panel_state) {
			if (!alpm_status_func(CHECK_PREVIOUS_STATUS)\
					&& alpm_status_func(CHECK_CURRENT_STATUS)) {
				/* Turn On ALPM Mode */
				mdss_samsung_send_cmd(ctrl, PANEL_ALPM_ON);
				msleep(20); /* wait 1 frame(more than 16ms) */
				mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_ON);
				alpm_status_func(STORE_CURRENT_STATUS);
				LCD_INFO("[ALPM_DEBUG] Send ALPM mode on cmds\n");
			}
		}
	} else if (mode == MODE_OFF) {
		if (alpm_status_func(CHECK_PREVIOUS_STATUS) == ALPM_MODE_ON) {
				if (pinfo->panel_state) {
					mdss_samsung_send_cmd(ctrl, PANEL_ALPM_OFF);
					msleep(20); /* wait 1 frame(more than 16ms) */
					mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_ON);
					alpm_status_func(CLEAR_MODE_STATUS);

					mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
					pdata->set_backlight(pdata, vdd->bl_level);
					mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

					LCD_INFO("[ALPM_DEBUG] Send ALPM off cmds\n");
				}
			}
	} else
		LCD_INFO("[ALPM_DEBUG] no operation \n:");

	return size;
}
static ssize_t mdss_samsung_alpm_backlight_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	LCD_DEBUG("TDB");

	return 0;
}

static ssize_t mdss_samsung_alpm_backlight_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	LCD_DEBUG("TDB");

	return size;
}

int hmt_bright_update(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_data *pdata;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	msleep(20);

	if (vdd->hmt_stat.hmt_on) {
		mdss_samsung_brightness_dcs_hmt(ctrl, vdd->hmt_stat.hmt_bl_level);
	} else {
		mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		pdata->set_backlight(pdata, vdd->bl_level);
		mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		LCD_INFO("hmt off state!\n");
	}

	return 0;
}

int hmt_enable(struct mdss_dsi_ctrl_pdata *ctrl, int enable)
{
	if (enable) {
		LCD_INFO("Single Scan Enable ++ \n");
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_ENABLE);
		LCD_INFO("Single Scan Enable -- \n");
	} else {
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_DISABLE);
		LCD_INFO("HMT OFF.. \n");
	}

	return 0;
}

int hmt_reverse_update(struct mdss_dsi_ctrl_pdata *ctrl, int enable)
{
	if (enable) {
		LCD_INFO("REVERSE ENABLE ++\n");
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_REVERSE_ENABLE);
		LCD_INFO("REVERSE ENABLE --\n");
	} else {
		LCD_INFO("REVERSE DISABLE ++ \n");
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_REVERSE_DISABLE);
		LCD_INFO("REVERSE DISABLE -- \n");
	}

	return 0;
}

static ssize_t mipi_samsung_hmt_bright_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		LCD_ERR("hmt is not supported..\n");
		return -ENODEV;
	}

	rc = snprintf((char *)buf, 30, "%d\n", vdd->hmt_stat.hmt_bl_level);
	LCD_INFO("[HMT] hmt bright : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_bright_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl;

	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		LCD_ERR("hmt is not supported..\n");
		return -ENODEV;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	pinfo = &ctrl->panel_data.panel_info;

	sscanf(buf, "%d" , &input);
	LCD_INFO("[HMT] input (%d) ++\n", input);

	if (!vdd->hmt_stat.hmt_on) {
		LCD_INFO("[HMT] hmt is off!\n");
		return size;
	}

	if (!pinfo->blank_state) {
		LCD_ERR("[HMT] panel is off!\n");
		vdd->hmt_stat.hmt_bl_level = input;
		return size;
	}

	if (vdd->hmt_stat.hmt_bl_level == input) {
		LCD_ERR("[HMT] hmt bright already %d!\n", vdd->hmt_stat.hmt_bl_level);
		return size;
	}

	vdd->hmt_stat.hmt_bl_level = input;
	hmt_bright_update(ctrl);

	LCD_INFO("[HMT] input (%d) --\n", input);

	return size;
}

static ssize_t mipi_samsung_hmt_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		LCD_ERR("hmt is not supported..\n");
		return -ENODEV;
	}

	rc = snprintf((char *)buf, 30, "%d\n", vdd->hmt_stat.hmt_on);
	LCD_INFO("[HMT] hmt on input : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl;

	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		LCD_ERR("hmt is not supported..\n");
		return -ENODEV;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	pinfo = &ctrl->panel_data.panel_info;

	sscanf(buf, "%d" , &input);
	LCD_INFO("[HMT] input (%d) ++\n", input);

	if (!pinfo->blank_state) {
		LCD_ERR("[HMT] panel is off!\n");
		vdd->hmt_stat.hmt_on = input;
		return size;
	}

	if (vdd->hmt_stat.hmt_on == input) {
		LCD_INFO("[HMT] hmt already %s !\n", vdd->hmt_stat.hmt_on?"ON":"OFF");
		return size;
	}

	vdd->hmt_stat.hmt_on = input;

	if (vdd->hmt_stat.hmt_on && vdd->hmt_stat.hmt_low_persistence) {
		hmt_enable(ctrl, 1);
		hmt_reverse_update(ctrl, 1);
	} else {
		hmt_enable(ctrl, 0);
		hmt_reverse_update(ctrl, 0);
	}

	hmt_bright_update(ctrl);

	LCD_INFO("[HMT] input hmt (%d) hmt lp (%d)--\n", input, vdd->hmt_stat.hmt_low_persistence);

	return size;
}

static ssize_t mipi_samsung_hmt_low_persistence_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		LCD_ERR(": hmt is not supported..\n");
		return -ENODEV;
	}

	rc = snprintf((char *)buf, 30, "%d\n", vdd->hmt_stat.hmt_low_persistence);
	LCD_INFO("[HMT] hmt low persistence : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_low_persistence_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl;

	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		LCD_ERR("hmt is not supported..\n");
		return -ENODEV;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	pinfo = &ctrl->panel_data.panel_info;

	sscanf(buf, "%d" , &input);
	LCD_INFO("[HMT] input (%d) ++\n", input);

	if (!vdd->hmt_stat.hmt_on) {
		LCD_INFO("[HMT] hmt is off!\n");
		return size;
	}

	if (vdd->hmt_stat.hmt_low_persistence == input) {
		LCD_ERR("[HMT] already low_persistence (%d)\n",
			vdd->hmt_stat.hmt_low_persistence);
		return size;
	}

	if (!pinfo->blank_state) {
		LCD_ERR("[HMT] panel is off!\n");
		vdd->hmt_stat.hmt_low_persistence = input;
		return size;
	}

	vdd->hmt_stat.hmt_low_persistence = input;

	if (!vdd->hmt_stat.hmt_low_persistence) {
		hmt_enable(ctrl, 0);
		hmt_reverse_update(ctrl, 0);
	} else {
		hmt_enable(ctrl, 1);
		hmt_reverse_update(ctrl, 1);
	}

	hmt_bright_update(ctrl);

	LCD_INFO("[HMT] input hmt (%d) hmt lp (%d)-- \n", vdd->hmt_stat.hmt_on, vdd->hmt_stat.hmt_low_persistence);

	return size;
}

void mdss_samsung_cabc_update()
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return;
	}

	if(vdd->auto_brightness) {
		LCD_INFO("auto brightness is on , cabc cmds are already sent--\n");
		return;
	}

	if(vdd->siop_status) {
		if(vdd->panel_func.samsung_lvds_write_reg)
			vdd->panel_func.samsung_brightness_tft_pwm(vdd->ctrl_dsi[DISPLAY_1],vdd->bl_level);
		else {
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_OFF_DUTY);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_ON);
			if(vdd->dtsi_data[0].cabc_delay && !vdd->display_ststus_dsi[0].disp_on_pre)
				usleep(vdd->dtsi_data[0].cabc_delay);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_ON_DUTY);
		}
	} else {
		if(vdd->panel_func.samsung_lvds_write_reg)
			vdd->panel_func.samsung_brightness_tft_pwm(vdd->ctrl_dsi[DISPLAY_1],vdd->bl_level);
		else {
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_OFF_DUTY);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_OFF);
		}
	}
}

int config_cabc(int value)
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is error");
		return value;
	}

	if(vdd->siop_status == value) {
		LCD_INFO("No change in cabc state, update not needed--\n");
		return value;
	}

	vdd->siop_status = value;
	mdss_samsung_cabc_update();
	return 0;
}

static DEVICE_ATTR(lcd_type, S_IRUGO, mdss_samsung_disp_lcdtype_show, NULL);
static DEVICE_ATTR(cell_id, S_IRUGO, mdss_samsung_disp_cell_id_show, NULL);
static DEVICE_ATTR(window_type, S_IRUGO, mdss_samsung_disp_windowtype_show, NULL);
static DEVICE_ATTR(manufacture_date, S_IRUGO, mdss_samsung_disp_manufacture_date_show, NULL);
static DEVICE_ATTR(manufacture_code, S_IRUGO, mdss_samsung_disp_manufacture_code_show, NULL);
static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_disp_acl_show, mdss_samsung_disp_acl_store);
static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_disp_siop_show, mdss_samsung_disp_siop_store);
static DEVICE_ATTR(read_mtp, S_IRUGO | S_IWUSR | S_IWGRP, NULL, mdss_samsung_read_mtp_store);
static DEVICE_ATTR(temperature, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_temperature_show, mdss_samsung_temperature_store);
static DEVICE_ATTR(aid_log, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_aid_log_show, NULL);
static DEVICE_ATTR(partial_disp, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_disp_partial_disp_show, mdss_samsung_disp_partial_disp_store);
static DEVICE_ATTR(alpm, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_alpm_show, mdss_samsung_alpm_store);
static DEVICE_ATTR(alpm_backlight, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_alpm_backlight_show, mdss_samsung_alpm_backlight_store);
static DEVICE_ATTR(hmt_bright, S_IRUGO | S_IWUSR | S_IWGRP, mipi_samsung_hmt_bright_show, mipi_samsung_hmt_bright_store);
static DEVICE_ATTR(hmt_on, S_IRUGO | S_IWUSR | S_IWGRP,	mipi_samsung_hmt_on_show, mipi_samsung_hmt_on_store);
static DEVICE_ATTR(hmt_low_persistence, S_IRUGO | S_IWUSR | S_IWGRP, mipi_samsung_hmt_low_persistence_show, mipi_samsung_hmt_low_persistence_store);

static struct attribute *panel_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_cell_id.attr,
	&dev_attr_window_type.attr,
	&dev_attr_manufacture_date.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_power_reduce.attr,
	&dev_attr_siop_enable.attr,
	&dev_attr_aid_log.attr,
	&dev_attr_read_mtp.attr,
	&dev_attr_temperature.attr,
	&dev_attr_partial_disp.attr,
	&dev_attr_alpm.attr,
	&dev_attr_alpm_backlight.attr,
	&dev_attr_hmt_bright.attr,
	&dev_attr_hmt_on.attr,
	&dev_attr_hmt_low_persistence.attr,
	NULL
};
static const struct attribute_group panel_sysfs_group = {
	.attrs = panel_sysfs_attributes,
};

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_samsung_auto_brightness_show,
			mdss_samsung_auto_brightness_store);
static DEVICE_ATTR(weakness_hbm_comp, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_samsung_weakness_hbm_comp_show,
			mdss_samsung_weakness_hbm_comp_store);

static struct attribute *bl_sysfs_attributes[] = {
	&dev_attr_auto_brightness.attr,
	&dev_attr_weakness_hbm_comp.attr,
	NULL
};
static const struct attribute_group bl_sysfs_group = {
	.attrs = bl_sysfs_attributes,
};
#endif /* END CONFIG_LCD_CLASS_DEVICE*/

static ssize_t csc_read_cfg(struct device *dev,
               struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", csc_update);
	return ret;
}

static ssize_t csc_write_cfg(struct device *dev,
               struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int err;
	int mode;

	err =  kstrtoint(buf, 0, &mode);
	if (err)
	       return ret;

	csc_update = (u8)mode;
	csc_change = 1;
	LCD_INFO("csc ctrl set to csc_update(%d)\n", csc_update);

	return ret;
}

static DEVICE_ATTR(csc_cfg, S_IRUGO | S_IWUSR, csc_read_cfg, csc_write_cfg);

int mdss_samsung_create_sysfs(void *data)
{
	static int sysfs_enable = 0;
	int rc = 0;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd = NULL;
#endif
#endif
	struct device *csc_dev = vdd_data.mfd_dsi[0]->fbi->dev;

	/* sysfs creat func should be called one time in dual dsi mode */
	if (sysfs_enable)
		return 0;

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", NULL, data, NULL);

	if (IS_ERR_OR_NULL(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		LCD_ERR("Failed to register lcd device..\n");
		return rc;
	}

	rc = sysfs_create_group(&lcd_device->dev.kobj, &panel_sysfs_group);
	if (rc) {
		LCD_ERR("Failed to create panel sysfs group..\n");
		sysfs_remove_group(&lcd_device->dev.kobj, &panel_sysfs_group);
		return rc;
	}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
						data, NULL, NULL);
	if (IS_ERR(bd)) {
		rc = PTR_ERR(bd);
		LCD_ERR("backlight : failed to register device\n");
		return rc;
	}

	rc = sysfs_create_group(&bd->dev.kobj, &bl_sysfs_group);
	if (rc) {
		LCD_ERR("Failed to create backlight sysfs group..\n");
		sysfs_remove_group(&bd->dev.kobj, &bl_sysfs_group);
		return rc;
	}
#endif

	rc = sysfs_create_file(&lcd_device->dev.kobj, &dev_attr_tuning.attr);
	if (rc) {
		LCD_ERR("sysfs create fail-%s\n", dev_attr_tuning.attr.name);
		return rc;
	}
#endif

	rc = sysfs_create_file(&csc_dev->kobj, &dev_attr_csc_cfg.attr);
	if (rc) {
		LCD_ERR("sysfs create fail-%s\n", dev_attr_csc_cfg.attr.name);
		return rc;
	}

	sysfs_enable = 1;

	LCD_INFO("done!!\n");

	return rc;
}

struct samsung_display_driver_data *samsung_get_vdd(void)
{
	return &vdd_data;
}

int display_ndx_check(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return DSI_CTRL_0;
	}

	if (vdd->support_hall_ic) {
		if (vdd->display_ststus_dsi[DISPLAY_1].hall_ic_status == HALL_IC_OPEN)
			return DSI_CTRL_0; /*OPEN : Internal PANEL */
		else
			return DSI_CTRL_1; /*CLOSE : External PANEL */
	} else
		return ctrl->ndx;
}

int samsung_display_hall_ic_status(struct notifier_block *nb,
			unsigned long hall_ic, void *data)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = vdd_data.ctrl_dsi[DISPLAY_1];

	/*
		previou panel off -> current panel on

		foder open : 0, close : 1
	*/

	if (!vdd_data.support_hall_ic)
		return 0;

	mutex_lock(&vdd_data.vdd_blank_unblank_lock); /*blank mode change */
	mutex_lock(&vdd_data.vdd_hall_ic_lock); /* HALL IC switching */

	if (get_lcd_attached("GET") && get_lcd_attached_secondary("GET")) {
	/* To check current blank mode */
	if (vdd_data.vdd_blank_mode[DISPLAY_1] == FB_BLANK_UNBLANK && \
		ctrl_pdata->panel_data.panel_info.panel_state) {
			vdd_data.display_ststus_dsi[DISPLAY_1].hall_ic_mode_change_trigger = true;

		/* panel off */
		ctrl_pdata->off(&ctrl_pdata->panel_data);

		vdd_data.display_ststus_dsi[DISPLAY_1].hall_ic_status = hall_ic;

		/* panel on */
		ctrl_pdata->on(&ctrl_pdata->panel_data);

		/* display on */
		mdss_samsung_send_cmd(ctrl_pdata, PANEL_DISPLAY_ON);
			vdd_data.display_ststus_dsi[DISPLAY_1].hall_ic_mode_change_trigger = false;
	} else {
		vdd_data.display_ststus_dsi[DISPLAY_1].hall_ic_status = hall_ic;
		LCD_ERR("skip display changing\n");
	}
	} else {
		if (get_lcd_attached("GET"))
			vdd_data.display_ststus_dsi[DISPLAY_1].hall_ic_status = HALL_IC_OPEN;

		if (get_lcd_attached_secondary("GET"))
			vdd_data.display_ststus_dsi[DISPLAY_1].hall_ic_status = HALL_IC_CLOSE;
	}

	mutex_unlock(&vdd_data.vdd_hall_ic_lock); /* HALL IC switching */
	mutex_unlock(&vdd_data.vdd_blank_unblank_lock); /*blank mode change */

	LCD_ERR("mdss hall_ic : %s, blank_status: %d end\n", hall_ic ? "CLOSE" : "OPEN", vdd_data.vdd_blank_mode[DISPLAY_1]);

	return 0;
}

/************************************************************
*
*		MDSS & DSI REGISTER DUMP FUNCTION
*
**************************************************************/
size_t kvaddr_to_paddr(unsigned long vaddr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	size_t paddr;

	pgd = pgd_offset_k(vaddr);
	if (unlikely(pgd_none(*pgd) || pgd_bad(*pgd)))
		return 0;

	pud = pud_offset(pgd, vaddr);
	if (unlikely(pud_none(*pud) || pud_bad(*pud)))
		return 0;

	pmd = pmd_offset(pud, vaddr);
	if (unlikely(pmd_none(*pmd) || pmd_bad(*pmd)))
		return 0;

	pte = pte_offset_kernel(pmd, vaddr);
	if (!pte_present(*pte))
		return 0;

	paddr = (unsigned long)pte_pfn(*pte) << PAGE_SHIFT;
	paddr += (vaddr & (PAGE_SIZE - 1));

	return paddr;
}

static void dump_reg(char *addr, int len)
{
	if (IS_ERR_OR_NULL(addr))
		return;

    mdss_dump_reg(MDSS_DBG_DUMP_IN_LOG, addr, len, NULL, false);
}

void mdss_samsung_dump_regs(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	char name[32];
	int loop;

	snprintf(name, sizeof(name), "MDP BASE");
	LCD_ERR("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->mdss_io.base));
	dump_reg(mdata->mdss_io.base, 0x100);

	snprintf(name, sizeof(name), "MDP REG");
	LCD_ERR("=============%s 0x%08zx ==============\n", name,
		kvaddr_to_paddr((unsigned long)mdata->mdp_base));
	dump_reg(mdata->mdp_base, 0x500);

	for(loop = 0; loop < mdata->nctl ; loop++) {
		snprintf(name, sizeof(name), "CTRL%d", loop);
		LCD_ERR("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->ctl_off[loop].base));
		dump_reg(mdata->ctl_off[loop].base, 0x200);
	}

	for(loop = 0; loop < mdata->nvig_pipes ; loop++) {
		snprintf(name, sizeof(name), "VG%d", loop);
		LCD_ERR("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->vig_pipes[loop].base));
		dump_reg(mdata->vig_pipes[loop].base, 0x100);
	}

	for(loop = 0; loop < mdata->nrgb_pipes ; loop++) {
		snprintf(name, sizeof(name), "RGB%d", loop);
		LCD_ERR("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->rgb_pipes[loop].base));
		dump_reg(mdata->rgb_pipes[loop].base, 0x100);
	}

	for(loop = 0; loop < mdata->ndma_pipes ; loop++) {
		snprintf(name, sizeof(name), "DMA%d", loop);
		LCD_ERR("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->dma_pipes[loop].base));
		dump_reg(mdata->dma_pipes[loop].base, 0x100);
	}

	for(loop = 0; loop < mdata->nmixers_intf ; loop++) {
		snprintf(name, sizeof(name), "MIXER_INTF_%d", loop);
		LCD_ERR("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->mixer_intf[loop].base));
		dump_reg(mdata->mixer_intf[loop].base, 0x100);
	}

	for(loop = 0; loop < mdata->nmixers_wb ; loop++) {
		snprintf(name, sizeof(name), "MIXER_WB_%d", loop);
		LCD_ERR("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->mixer_wb[loop].base));
		dump_reg(mdata->mixer_wb[loop].base, 0x100);
	}

	for(loop = 0; loop < mdata->nmixers_intf ; loop++) {
		snprintf(name, sizeof(name), "PING_PONG%d", loop);
		LCD_ERR("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->mixer_intf[loop].pingpong_base));
		dump_reg(mdata->mixer_intf[loop].pingpong_base, 0x40);
	}

#if defined(CONFIG_ARCH_MSM8992)
	/* To dump ping-pong slave register for ping-pong split supporting chipset */
	snprintf(name, sizeof(name), "PING_PONG SLAVE");
		LCD_ERR("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->slave_pingpong_base));
		dump_reg(mdata->slave_pingpong_base, 0x40);
#endif

}

void mdss_samsung_dsi_dump_regs(int dsi_num)
{
	struct mdss_dsi_ctrl_pdata **dsi_ctrl = mdss_dsi_get_ctrl();
	char name[32];

	if (vdd_data.panel_attach_status & BIT(dsi_num)) {
	snprintf(name, sizeof(name), "DSI%d CTL", dsi_num);
	LCD_ERR("=============%s 0x%08zx ==============\n", name,
		kvaddr_to_paddr((unsigned long)dsi_ctrl[dsi_num]->ctrl_io.base));
		dump_reg((char *)dsi_ctrl[dsi_num]->ctrl_io.base, dsi_ctrl[dsi_num]->ctrl_io.len);

	snprintf(name, sizeof(name), "DSI%d PHY", dsi_num);
	LCD_ERR("=============%s 0x%08zx ==============\n", name,
		kvaddr_to_paddr((unsigned long)dsi_ctrl[dsi_num]->phy_io.base));
		dump_reg((char *)dsi_ctrl[dsi_num]->phy_io.base, (size_t)dsi_ctrl[dsi_num]->phy_io.len);

	snprintf(name, sizeof(name), "DSI%d PLL", dsi_num);
	LCD_ERR("=============%s 0x%08zx ==============\n", name,
		kvaddr_to_paddr((unsigned long)vdd_data.dump_info[dsi_num].dsi_pll.virtual_addr));
		dump_reg((char *)vdd_data.dump_info[dsi_num].dsi_pll.virtual_addr, 0x200);

#if defined(CONFIG_ARCH_MSM8992) || defined(CONFIG_ARCH_MSM8994)
	snprintf(name, sizeof(name), "DSI%d REGULATOR", dsi_num);
	LCD_ERR("=============%s 0x%08zx ==============\n", name,
		kvaddr_to_paddr((unsigned long)dsi_ctrl[dsi_num]->shared_ctrl_data->phy_regulator_io.base));
	dump_reg((char *)dsi_ctrl[dsi_num]->shared_ctrl_data->phy_regulator_io.base, (size_t)dsi_ctrl[dsi_num]->shared_ctrl_data->phy_regulator_io.len);
#endif
	}
}

int mdss_samsung_read_rddpm(void)
{
	struct mdss_dsi_ctrl_pdata **dsi_ctrl = mdss_dsi_get_ctrl();
	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	char rddpm_reg = 0;

	MDSS_XLOG(dsi_ctrl[DISPLAY_1]->ndx, 0x0A);

	if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].ldi_debug0_rx_cmds[vdd->panel_revision].cmds)) {
		mdss_samsung_read_nv_mem(dsi_ctrl[DISPLAY_1], &vdd->dtsi_data[DISPLAY_1].ldi_debug0_rx_cmds[vdd->panel_revision], &rddpm_reg, 0);

		LCD_ERR("rddpm 0x(%x)\n", rddpm_reg);

		if (rddpm_reg == 0x08) {
			LCD_ERR("ddi reset status (%x)\n", rddpm_reg);
			schedule_work(&pstatus_data->check_status.work);
			return 1;
		}
	} else
		LCD_ERR("no rddpm read cmds..\n");

	return 0;
}

int mdss_samsung_dsi_te_check(void)
{
	struct mdss_dsi_ctrl_pdata **dsi_ctrl = mdss_dsi_get_ctrl();
	int rc, te_count = 0;
	int te_max = 20000; /*sampling 200ms */

	if (dsi_ctrl[DISPLAY_1]->panel_mode == DSI_VIDEO_MODE)
		return 0;

	if (gpio_is_valid(dsi_ctrl[DISPLAY_1]->disp_te_gpio)) {
		LCD_ERR("============ start waiting for TE ============\n");

		for (te_count = 0;  te_count < te_max; te_count++) {
			rc = gpio_get_value(dsi_ctrl[DISPLAY_1]->disp_te_gpio);
			if (rc == 1) {
				LCD_ERR("gpio_get_value(disp_te_gpio) = %d ", rc);
				LCD_ERR("te_count = %d\n", te_count);
				break;
			}
			/* usleep suspends the calling thread whereas udelay is a
			 * busy wait. Here the value of te_gpio is checked in a loop of
			 * max count = 250. If this loop has to iterate multiple
			 * times before the te_gpio is 1, the calling thread will end
			 * up in suspend/wakeup sequence multiple times if usleep is
			 * used, which is an overhead. So use udelay instead of usleep.
			 */
			udelay(10);
		}

		if (te_count == te_max) {
			LCD_ERR("LDI doesn't generate TE, ddi recovery start.\n");
			return 1;
		} else
			LCD_ERR("LDI generate TE\n");

		LCD_ERR("============ finish waiting for TE ============\n");
	} else
		LCD_ERR("disp_te_gpio is not valid\n");

	return 0;
}

void mdss_mdp_underrun_dump_info(void)
{
	struct mdss_mdp_pipe *pipe;
	/* struct mdss_data_type *mdss_res = mdss_mdp_get_mdata(); */
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(vdd_data.mfd_dsi[0]);
	int pcount = mdp5_data->mdata->nrgb_pipes + mdp5_data->mdata->nvig_pipes + mdp5_data->mdata->ndma_pipes;

	LCD_ERR(" ============ start ===========\n");
	list_for_each_entry(pipe, &mdp5_data->pipes_used, list) {
		if (pipe && pipe->src_fmt)
			LCD_ERR("[%4d, %4d, %4d, %4d] -> [%4d, %4d, %4d, %4d]"
				"|flags = %8d|src_format = %2d|bpp = %2d|ndx = %3d|\n",
				pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
				pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h,
				pipe->flags, pipe->src_fmt->format, pipe->src_fmt->bpp,
				pipe->ndx);
		LCD_ERR("pipe addr : %p\n", pipe);
		pcount--;
		if (!pcount) break;
	}
/*
	pr_err("bus_ab = %llu, bus_ib = %llu\n",
		mdss_res->bus_scale_table->usecase[mdss_res->curr_bw_uc_idx].vectors[0].ab,
		mdss_res->bus_scale_table->usecase[mdss_res->curr_bw_uc_idx].vectors[0].ib);
*/
	LCD_ERR("============ end ===========\n");
}

DEFINE_MUTEX(FENCE_LOCK);
void mdss_samsung_fence_dump(struct sync_fence *fence)
{
	struct sync_pt *pt;
	struct list_head *pos;

	struct timeval tv;
	int status;

	mutex_lock(&FENCE_LOCK);

	dump_stack();
	LCD_ERR("fence name : %s\n", fence->name);

	list_for_each(pos, &fence->pt_list_head) {
		pt = container_of(pos, struct sync_pt, pt_list);
		tv = ktime_to_timeval(pt->timestamp);
		status = pt->status;

		pr_err("%s %s%spt %s : %ld.%06ld\n", __func__,
		   fence ? pt->parent->name : "",
		   fence ? "_" : "",
		   status > 0 ? "signaled" : pt->status == 0 ? "active" : "error",
		   tv.tv_sec, tv.tv_usec);
	}

	mutex_unlock(&FENCE_LOCK);
}
