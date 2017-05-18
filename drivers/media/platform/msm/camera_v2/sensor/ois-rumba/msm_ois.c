/* Copyright (c) 2014 - 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef  pr_fmt
#define pr_fmt(fmt) " [sz_cam_ois] MSM %s() " fmt, __func__

#include <linux/module.h>
#include <linux/firmware.h>
#include "msm_sd.h"
#include "msm_ois.h"
#include "msm_cci.h"
/*ASUS_BSP +++ bill_chen "Implement ois"*/
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/clk.h>
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
/*ASUS_BSP --- bill_chen "Implement ois"*/
#include "asus_ois.h"
#include "rumba_interface.h"
#include "rumba_i2c.h"
#include "utils.h"

DEFINE_MSM_MUTEX(msm_ois_mutex);
//#define MSM_OIS_DEBUG
#undef CDBG
#ifdef MSM_OIS_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

static struct v4l2_file_operations msm_ois_v4l2_subdev_fops;
static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *o_ctrl);
static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *o_ctrl);

static struct i2c_driver msm_ois_i2c_driver;

/*ASUS_BSP +++ bill_chen "Implement ois"*/
static int32_t msm_ois_check_id(struct msm_ois_ctrl_t *o_ctrl);
//static int32_t msm_ois_check_vm(struct msm_ois_ctrl_t *o_ctrl);


static int32_t msm_ois_get_dt_data(struct msm_ois_ctrl_t *o_ctrl);
static struct msm_ois_ctrl_t *msm_ois_t_pointer = NULL;
/*ASUS_BSP --- bill_chen "Implement ois"*/
#if 0
static int32_t msm_ois_download(struct msm_ois_ctrl_t *o_ctrl)
{
	uint16_t bytes_in_tx = 0;
	uint16_t total_bytes = 0;
	uint8_t *ptr = NULL;
	int32_t rc = 0;
	const struct firmware *fw = NULL;
	const char *fw_name_prog = NULL;
	const char *fw_name_coeff = NULL;
	char name_prog[MAX_SENSOR_NAME] = {0};
	char name_coeff[MAX_SENSOR_NAME] = {0};
	struct device *dev = &(o_ctrl->pdev->dev);
	enum msm_camera_i2c_reg_addr_type save_addr_type;

	CDBG("Enter\n");
	save_addr_type = o_ctrl->i2c_client.addr_type;
	o_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

	snprintf(name_coeff, MAX_SENSOR_NAME, "%s.coeff",
		o_ctrl->oboard_info->ois_name);

	snprintf(name_prog, MAX_SENSOR_NAME, "%s.prog",
		o_ctrl->oboard_info->ois_name);

	/* cast pointer as const pointer*/
	fw_name_prog = name_prog;
	fw_name_coeff = name_coeff;

	/* Load FW */
	rc = request_firmware(&fw, fw_name_prog, dev);
	if (rc) {
		dev_err(dev, "Failed to locate %s\n", fw_name_prog);
		o_ctrl->i2c_client.addr_type = save_addr_type;
		return rc;
	}

	total_bytes = fw->size;
	for (ptr = (uint8_t *)fw->data; total_bytes;
		total_bytes -= bytes_in_tx, ptr += bytes_in_tx) {
		bytes_in_tx = (total_bytes > 10) ? 10 : total_bytes;
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(
			&o_ctrl->i2c_client, o_ctrl->oboard_info->opcode.prog,
			 ptr, bytes_in_tx);
		if (rc < 0) {
			pr_err("Failed: remaining bytes to be downloaded: %d",
				bytes_in_tx);
			/* abort download fw and return error*/
			goto release_firmware;
		}
	}
	release_firmware(fw);

	rc = request_firmware(&fw, fw_name_coeff, dev);
	if (rc) {
		dev_err(dev, "Failed to locate %s\n", fw_name_coeff);
		o_ctrl->i2c_client.addr_type = save_addr_type;
		return rc;
	}
	total_bytes = fw->size;
	for (ptr = (uint8_t *)fw->data; total_bytes;
		total_bytes -= bytes_in_tx, ptr += bytes_in_tx) {
		bytes_in_tx = (total_bytes > 10) ? 10 : total_bytes;
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(
			&o_ctrl->i2c_client, o_ctrl->oboard_info->opcode.coeff,
			ptr, bytes_in_tx);
		if (rc < 0) {
			pr_err("Failed: remaining bytes to be downloaded: %d",
				total_bytes);
			/* abort download fw*/
			break;
		}
	}
release_firmware:
	release_firmware(fw);
	o_ctrl->i2c_client.addr_type = save_addr_type;

	return rc;
}

static int32_t msm_ois_data_config(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_slave_info *slave_info)
{
	int rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;

	CDBG("Enter\n");
	if (!slave_info) {
		pr_err("failed : invalid slave_info ");
		return -EINVAL;
	}
	/* fill ois slave info*/
	if (strlcpy(o_ctrl->oboard_info->ois_name, slave_info->ois_name,
		sizeof(o_ctrl->oboard_info->ois_name)) < 0) {
		pr_err("failed: copy_from_user");
		return -EFAULT;
	}
	memcpy(&(o_ctrl->oboard_info->opcode), &(slave_info->opcode),
		sizeof(struct msm_ois_opcode));
	o_ctrl->oboard_info->i2c_slaveaddr = slave_info->i2c_addr;

	/* config cci_client*/
	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = o_ctrl->i2c_client.cci_client;
		cci_client->sid =
			o_ctrl->oboard_info->i2c_slaveaddr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->cci_i2c_master = o_ctrl->cci_master;
	} else {
		o_ctrl->i2c_client.client->addr =
			o_ctrl->oboard_info->i2c_slaveaddr;
	}

	CDBG("Exit\n");
	return rc;
}
#endif
static int32_t msm_ois_write_settings(struct msm_ois_ctrl_t *o_ctrl,
	uint16_t size, struct reg_settings_ois_t *settings)
{
	int32_t rc = -EFAULT;
	int32_t i = 0;
	/*ASUS_BSP +++ bill_chen "Implement ois"*/

	uint32_t read_num = 0;

	/*ASUS_BSP --- bill_chen "Implement ois"*/
	CDBG("Enter\n");

	for (i = 0; i < size; i++) {
		switch (settings[i].i2c_operation) {
		case MSM_OIS_WRITE: {
			switch (settings[i].data_type) {
			case MSM_CAMERA_I2C_BYTE_DATA:
				pr_info("msm_ois_write_settings [%d], write 0x%X to reg 0x%X\n",
				    i,settings[i].reg_data,settings[i].reg_addr
					);
				rc = rumba_write_byte(o_ctrl,settings[i].reg_addr,(uint16_t)settings[i].reg_data);
				break;
			case MSM_CAMERA_I2C_WORD_DATA:
				pr_info("msm_ois_write_settings [%d], write 0x%X to reg 0x%X\n",
				    i,settings[i].reg_data,settings[i].reg_addr
					);
				rc = rumba_write_word(o_ctrl,settings[i].reg_addr,(uint16_t)settings[i].reg_data);
				break;
			case MSM_CAMERA_I2C_DWORD_DATA:
				pr_info("msm_ois_write_settings [%d], write 0x%X to reg 0x%X\n",
				    i,settings[i].reg_data,settings[i].reg_addr
					);
				rc = rumba_write_dword(o_ctrl,settings[i].reg_addr,settings[i].reg_data);
				break;

			/*ASUS_BSP +++ bill_chen "Implement ois"*/
			case MSM_CAMERA_I2C_WRITE_EEPROM_DATA:

				rc = 0;
				break;
			case MSM_CAMERA_I2C_WRITE_CALIBRATION_DATA:
				rc = 0;
				break;
			case MSM_CAMERA_I2C_NO_DATA:
				break;
			case MSM_CAMERA_I2C_WRITE_FW_DATA:
				rc = 0;
				break;
			/*ASUS_BSP --- bill_chen "Implement ois"*/

			default:
				pr_err("Unsupport data type: %d\n",
					settings[i].data_type);
				break;
			}
			if (settings[i].delay > 20)
			{
				pr_info("msm_ois_write_settings, write delay %d ms\n",settings[i].delay);
				msleep(settings[i].delay);
				pr_info("msm_ois_write_settings, write delay %d ms done\n",settings[i].delay);
			}
			else if (0 != settings[i].delay)
				usleep_range(settings[i].delay * 1000,
					(settings[i].delay * 1000) + 1000);
		}
			break;

	    /*ASUS_BSP +++ bill_chen "Implement ois"*/
		case MSM_OIS_READ: {
			switch (settings[i].data_type) {
			case MSM_CAMERA_I2C_READ_EEPROM_DATA:
				rc = 0;
				break;
			case MSM_CAMERA_I2C_READ_MODE_DATA:
				rc = 0;
				break;
			case MSM_CAMERA_I2C_READ_FW_DATA:

				rc = 0;
				break;
			case MSM_CAMERA_I2C_READ_CALIBRATION_DATA:

				rc = 0;
				break;
			case MSM_CAMERA_I2C_BYTE_DATA:
				rc = rumba_read_byte(o_ctrl,settings[i].reg_addr,(uint16_t*)&read_num);
				pr_info("msm_ois_write_setting [%d], read 0x%x from reg 0x%x\n",i, read_num, settings[i].reg_addr);
				if(settings[i].reg_data != read_num)
				{
					pr_err("read_num 0x%x not match expected value = 0x%x\n", read_num,settings[i].reg_data);
				}
				break;
			case MSM_CAMERA_I2C_WORD_DATA:
				rc = rumba_read_word(o_ctrl,settings[i].reg_addr,(uint16_t*)&read_num);
				pr_info("msm_ois_write_setting [%d], read 0x%x from reg 0x%x\n",i, read_num, settings[i].reg_addr);
				if(settings[i].reg_data != read_num)
				{
					pr_err("read_num 0x%x not match expected value = 0x%x\n", read_num,settings[i].reg_data);
				}
				break;
			case MSM_CAMERA_I2C_DWORD_DATA:
				rc = rumba_read_dword(o_ctrl,settings[i].reg_addr,&read_num);
				pr_info("msm_ois_write_setting [%d], read 0x%x from reg 0x%x\n",i, read_num, settings[i].reg_addr);
				if(settings[i].reg_data != read_num)
				{
					pr_err("read_num 0x%x not match expected value = 0x%x\n", read_num,settings[i].reg_data);
				}
				break;

			default:
				pr_err("Unsupport data type: %d\n",
					settings[i].data_type);
				break;
			}
			if (settings[i].delay > 20)
			{
				pr_info("msm_ois_write_settings, read delay %d ms\n",settings[i].delay);
				msleep(settings[i].delay);
				pr_info("msm_ois_write_settings, read delay %d ms done\n",settings[i].delay);
			}
			else if (0 != settings[i].delay)
				usleep_range(settings[i].delay * 1000,
					(settings[i].delay * 1000) + 1000);
		}
			break;
		/*ASUS_BSP --- bill_chen "Implement ois"*/


		case MSM_OIS_POLL: {
			switch (settings[i].data_type) {
			case MSM_CAMERA_I2C_BYTE_DATA:
				pr_info("msm_ois_write_settings [%d], poll 0x%X of reg 0x%X\n",
				    i,settings[i].reg_data,settings[i].reg_addr
					);
				rc = rumba_poll_byte(o_ctrl,settings[i].reg_addr,settings[i].reg_data,settings[i].delay);

				break;
			case MSM_CAMERA_I2C_WORD_DATA:
				pr_info("msm_ois_write_settings [%d], poll 0x%X of reg 0x%X\n",
				    i,settings[i].reg_data,settings[i].reg_addr
					);
				rc = rumba_poll_word(o_ctrl,settings[i].reg_addr,settings[i].reg_data,settings[i].delay);

				break;

			default:
				pr_err("Unsupport data type: %d\n",
					settings[i].data_type);
				break;
			}
		}
		}

		if (rc < 0)
			break;
	}

	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_vreg_control(struct msm_ois_ctrl_t *o_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_ois_vreg *vreg_cfg;

	vreg_cfg = &o_ctrl->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt)
		return 0;

	if (cnt >= MSM_OIS_MAX_VREGS) {
		pr_err("%s failed %d cnt %d\n", __func__, __LINE__, cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		rc = msm_camera_config_single_vreg(&(o_ctrl->pdev->dev),
			&vreg_cfg->cam_vreg[i],
			(struct regulator **)&vreg_cfg->data[i],
			config);
	}
	return rc;
}

static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *o_ctrl)
{
	int32_t rc = 0;
	//enum msm_sensor_power_seq_gpio_t gpio;  /*ASUS_BSP bill_chen "Fix ois conflict"*/

	CDBG("Enter\n");
	pr_info("%s: E\n", __func__);  /*ASUS_BSP bill_chen "Implement ois"*/
	if (o_ctrl->ois_state != OIS_DISABLE_STATE) {
		pr_info("Servo off ...\n");
		rumba_servo_go_off(o_ctrl);
		rc = msm_ois_vreg_control(o_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

#if 0
		for (gpio = SENSOR_GPIO_AF_PWDM; gpio < SENSOR_GPIO_MAX;
			gpio++) {
			if (o_ctrl->gconf &&
				o_ctrl->gconf->gpio_num_info &&
				o_ctrl->gconf->
					gpio_num_info->valid[gpio] == 1) {
				gpio_set_value_cansleep(
					o_ctrl->gconf->gpio_num_info
						->gpio_num[gpio],
					GPIOF_OUT_INIT_LOW);

				if (o_ctrl->cam_pinctrl_status) {
					rc = pinctrl_select_state(
						o_ctrl->pinctrl_info.pinctrl,
						o_ctrl->pinctrl_info.
							gpio_state_suspend);
					if (rc < 0)
						pr_err("ERR:%s:%d cannot set pin to suspend state: %d",
							__func__, __LINE__, rc);
					devm_pinctrl_put(
						o_ctrl->pinctrl_info.pinctrl);
				}
				o_ctrl->cam_pinctrl_status = 0;
				rc = msm_camera_request_gpio_table(
					o_ctrl->gconf->cam_gpio_req_tbl,
					o_ctrl->gconf->cam_gpio_req_tbl_size,
					0);
				if (rc < 0)
					pr_err("ERR:%s:Failed in selecting state in ois power down: %d\n",
						__func__, rc);
			}
		}
#endif
		o_ctrl->i2c_tbl_index = 0;
		o_ctrl->ois_state = OIS_OPS_INACTIVE;
		g_ois_power_state = 0;
	}
	g_ois_mode = 255;
	pr_info("%s: X\n", __func__);  /*ASUS_BSP bill_chen "Implement ois"*/
	CDBG("Exit\n");
	return rc;
}

static int msm_ois_init(struct msm_ois_ctrl_t *o_ctrl)
{
	int rc = 0;
	CDBG("Enter\n");

	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}

	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&o_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	o_ctrl->ois_state = OIS_OPS_ACTIVE;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_control(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_set_info_t *set_info)
{
	struct reg_settings_ois_t *settings = NULL;
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	CDBG("Enter\n");

	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = o_ctrl->i2c_client.cci_client;
		cci_client->sid =
			set_info->ois_params.i2c_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->cci_i2c_master = o_ctrl->cci_master;
		cci_client->i2c_freq_mode = set_info->ois_params.i2c_freq_mode;
		pr_info("i2c_freq_mode set to %d\n",set_info->ois_params.i2c_freq_mode);
	} else {
		o_ctrl->i2c_client.client->addr =
			set_info->ois_params.i2c_addr;
	}

	if (set_info->ois_params.setting_size > 0 &&
		set_info->ois_params.setting_size
		< MAX_OIS_REG_SETTINGS) {
		settings = kmalloc(
			sizeof(struct reg_settings_ois_t) *
			(set_info->ois_params.setting_size),
			GFP_KERNEL);
		if (settings == NULL) {
			pr_err("Error allocating memory\n");
			return -EFAULT;
		}
		if (copy_from_user(settings,
			(void *)set_info->ois_params.settings,
			set_info->ois_params.setting_size *
			sizeof(struct reg_settings_ois_t))) {
			kfree(settings);
			pr_err("Error copying\n");
			return -EFAULT;
		}

		rc = msm_ois_write_settings(o_ctrl,
			set_info->ois_params.setting_size,
			settings);
		kfree(settings);
		if (rc < 0) {
			pr_err("Error\n");
			return -EFAULT;
		}
	}

	CDBG("Exit\n");

	return rc;
}


static int32_t msm_ois_config(struct msm_ois_ctrl_t *o_ctrl,
	void __user *argp)
{
	struct msm_ois_cfg_data *cdata =
		(struct msm_ois_cfg_data *)argp;
	int32_t rc = 0;
	mutex_lock(o_ctrl->ois_mutex);
	//pr_info("Enter\n");
	//pr_info("%s type %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_OIS_INIT:
		rc = msm_ois_init(o_ctrl);
		if (rc < 0)
			pr_err("msm_ois_init failed %d\n", rc);
		break;
	case CFG_OIS_POWERDOWN:
		rc = msm_ois_power_down(o_ctrl);
		if (rc < 0)
			pr_err("msm_ois_power_down failed %d\n", rc);
		break;
	case CFG_OIS_POWERUP:
		rc = msm_ois_power_up(o_ctrl);
		if (rc < 0)
			pr_err("Failed ois power up%d\n", rc);
		break;
	case CFG_OIS_CONTROL:

		if(g_ois_reg_fw_version == 0x1163)
		{
			pr_info("CFG_OIS_CONTROL OIS has bad FW, not write any setting ...");
			break;
		}

		rc = msm_ois_control(o_ctrl, &cdata->cfg.set_info);
		if (rc < 0)
			pr_err("Failed ois control%d\n", rc);
		break;
	case CFG_OIS_I2C_WRITE_SEQ_TABLE: {

		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if(g_ois_reg_fw_version == 0x1163)
		{
			pr_info("CFG_OIS_I2C_WRITE_SEQ_TABLE OIS has bad FW, not write any setting ...");
			break;
		}

#ifdef CONFIG_COMPAT
		if (is_compat_task()) {
			memcpy(&conf_array,
				(void *)cdata->cfg.settings,
				sizeof(struct msm_camera_i2c_seq_reg_setting));
		} else
#endif
		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.settings,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		if(is_i2c_seq_setting_address_valid(&conf_array,0x04FC))
		{
			conf_array.reg_setting = reg_setting;

			rc = o_ctrl->i2c_client.i2c_func_tbl->
				i2c_write_seq_table(&o_ctrl->i2c_client,
				&conf_array);
#if 0
		  {
			static uint8_t enabled = 0xee;
			static uint8_t first_on_off = 0;
			uint8_t reg_val = 0xee;
			if(i2c_seq_setting_contain_address(&conf_array,0x0430,&reg_val) && first_on_off == 1)
			{
				uint16_t acc_x = 0xeeee, acc_y = 0xeeee;
				int i;

				int loop_times;
				int delay_time_ms;

				if(enabled)
				{
				   loop_times = 10;
				   delay_time_ms = 2;
				}
				else
				{
				   loop_times = 500;
				   delay_time_ms = 50;
				}

				rumba_fw_info_enable(o_ctrl);
				for(i=0;i<loop_times;i++)
				{
					rc = rumba_read_acc_position(o_ctrl,&acc_x,&acc_y);

					if(enabled)
					{
						if(acc_x!=0 && acc_y!=0)
						{
							pr_info("After Enable Shift Compensation & Set distance, Get ACC value (0x%04x,0x%04x), times %d\n",acc_x,acc_y,i+1);
							break;
						}
					}
					else
					{
						if(acc_x == 0 && acc_y ==0)
						{
							pr_info("After Disable Shift Compensation & Set distance, Get ACC value (0x%04x,0x%04x), times %d\n",acc_x,acc_y,i+1);
							break;
						}
					}
					delay_ms(delay_time_ms);
				}
				rumba_fw_info_disable(o_ctrl);
				first_on_off = 0;
			}
			else if(i2c_seq_setting_contain_address(&conf_array,0x0400,&enabled) && enabled != 0xee)
			{
				first_on_off = 1;
			}
		}
#endif
		}
		else
		{
			pr_err("Invalid address detected for Rumba!\n");
		}
		kfree(reg_setting);
		break;
	}
	/*ASUS_BSP +++ bill_chen "Implement ois command for dit 3A"*/
	case CFG_OIS_I2C_WRITE_MODE: {
		struct msm_camera_i2c_seq_reg_setting conf_array;

		if(g_ois_reg_fw_version == 0x1163)
		{
			pr_info("CFG_OIS_I2C_WRITE_MODE OIS has bad FW, not write any setting ...");
			break;
		}

#ifdef CONFIG_COMPAT
		if (is_compat_task()) {
			memcpy(&conf_array,
				(void *)cdata->cfg.settings,
				sizeof(struct msm_camera_i2c_seq_reg_setting));
		} else
#endif
		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.settings,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if(conf_array.delay == g_ois_mode) {
		    break;
		}

		rc = rumba_switch_mode(msm_ois_t_pointer,conf_array.delay);
		if(rc == 0)
		{
			g_ois_mode = conf_array.delay;
			pr_info("OIS mode changed to %d by HAL\n",g_ois_mode);
		}

		break;
	}
	/*ASUS_BSP --- bill_chen "Implement ois command for dit 3A"*/
	default:
		break;
	}
	mutex_unlock(o_ctrl->ois_mutex);
	//pr_info("Exit\n");
	return rc;
}
#if 0
static int32_t msm_ois_config_download(struct msm_ois_ctrl_t *o_ctrl,
	void __user *argp)
{
	struct msm_ois_cfg_download_data *cdata =
		(struct msm_ois_cfg_download_data *)argp;
	int32_t rc = 0;

	if (!o_ctrl || !cdata) {
		pr_err("failed: Invalid data\n");
		return -EINVAL;
	}
	mutex_lock(o_ctrl->ois_mutex);
	CDBG("Enter\n");
	CDBG("%s type %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_OIS_DATA_CONFIG:
		rc = msm_ois_data_config(o_ctrl, &cdata->slave_info);
		if (rc < 0)
			pr_err("Failed ois data config %d\n", rc);
		break;
	case CFG_OIS_DOWNLOAD:
		rc = msm_ois_download(o_ctrl);
		if (rc < 0)
			pr_err("Failed ois download %d\n", rc);
		break;
	default:
		break;
	}
	mutex_unlock(o_ctrl->ois_mutex);
	CDBG("Exit\n");
	return rc;
}
#endif

static int32_t msm_ois_get_subdev_id(struct msm_ois_ctrl_t *o_ctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = o_ctrl->pdev->id;
	else
		*subdev_id = o_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq = msm_camera_qup_i2c_write_seq,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_poll = msm_camera_qup_i2c_poll,
};

static int msm_ois_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_ois_ctrl_t *o_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	mutex_lock(o_ctrl->ois_mutex);
	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE &&
		o_ctrl->ois_state != OIS_DISABLE_STATE) {
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&o_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	o_ctrl->ois_state = OIS_DISABLE_STATE;
	mutex_unlock(o_ctrl->ois_mutex);
	CDBG("Exit\n");
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_ois_internal_ops = {
	.close = msm_ois_close,
};

static long msm_ois_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc;
	struct msm_ois_ctrl_t *o_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;
	CDBG("Enter\n");
	CDBG("%s:%d o_ctrl %pK argp %pK\n", __func__, __LINE__, o_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_ois_get_subdev_id(o_ctrl, argp);
	case VIDIOC_MSM_OIS_CFG:
		return msm_ois_config(o_ctrl, argp);
	//case VIDIOC_MSM_OIS_CFG_DOWNLOAD:
		//return msm_ois_config_download(o_ctrl, argp);
	case MSM_SD_SHUTDOWN:
		if (!o_ctrl->i2c_client.i2c_func_tbl) {
			pr_err("o_ctrl->i2c_client.i2c_func_tbl NULL\n");
			return -EINVAL;
		}
		mutex_lock(o_ctrl->ois_mutex);
		rc = msm_ois_power_down(o_ctrl);
		mutex_unlock(o_ctrl->ois_mutex);
		if (rc < 0) {
			pr_err("%s:%d OIS Power down failed\n",
				__func__, __LINE__);
		}
		return msm_ois_close(sd, NULL);
	default:
		return -ENOIOCTLCMD;
	}
}

static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *o_ctrl)
{
	int rc = 0;

	CDBG("%s called\n", __func__);
	pr_info("%s: E\n", __func__);  /*ASUS_BSP bill_chen "Implement ois"*/


	rc = msm_ois_vreg_control(o_ctrl, 1);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}

#if 0
	for (gpio = SENSOR_GPIO_AF_PWDM;
		gpio < SENSOR_GPIO_MAX; gpio++) {
		if (o_ctrl->gconf && o_ctrl->gconf->gpio_num_info &&
			o_ctrl->gconf->gpio_num_info->valid[gpio] == 1) {
			rc = msm_camera_request_gpio_table(
				o_ctrl->gconf->cam_gpio_req_tbl,
				o_ctrl->gconf->cam_gpio_req_tbl_size, 1);
			if (rc < 0) {
				pr_err("ERR:%s:Failed in selecting state for ois: %d\n",
					__func__, rc);
				return rc;
			}
			if (o_ctrl->cam_pinctrl_status) {
				rc = pinctrl_select_state(
					o_ctrl->pinctrl_info.pinctrl,
					o_ctrl->pinctrl_info.gpio_state_active);
				if (rc < 0)
					pr_err("ERR:%s:%d cannot set pin to active state: %d",
						__func__, __LINE__, rc);
			}

			gpio_set_value_cansleep(
				o_ctrl->gconf->gpio_num_info->gpio_num[gpio],
				1);
		}
	}
#endif
	delay_ms(130);//wait 100ms for OIS and 30ms for I2C
	o_ctrl->ois_state = OIS_ENABLE_STATE;
	g_ois_power_state = 1;
	g_ois_mode = 0;//servo_off
	pr_info("%s: X\n", __func__);  /*ASUS_BSP bill_chen "Implement ois"*/
	CDBG("Exit\n");
	return rc;
}

static struct v4l2_subdev_core_ops msm_ois_subdev_core_ops = {
	.ioctl = msm_ois_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_ois_subdev_ops = {
	.core = &msm_ois_subdev_core_ops,
};

static const struct i2c_device_id msm_ois_i2c_id[] = {
	{"qcom,ois", (kernel_ulong_t)NULL},
	{ }
};

static int32_t msm_ois_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_ois_ctrl_t *ois_ctrl_t = NULL;
	CDBG("Enter\n");

	if (client == NULL) {
		pr_err("msm_ois_i2c_probe: client is null\n");
		return -EINVAL;
	}

	ois_ctrl_t = kzalloc(sizeof(struct msm_ois_ctrl_t),
		GFP_KERNEL);
	if (!ois_ctrl_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		rc = -EINVAL;
		goto probe_failure;
	}

	CDBG("client = 0x%pK\n",  client);

	rc = of_property_read_u32(client->dev.of_node, "cell-index",
		&ois_ctrl_t->subdev_id);
	CDBG("cell-index %d, rc %d\n", ois_ctrl_t->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		goto probe_failure;
	}

	ois_ctrl_t->i2c_driver = &msm_ois_i2c_driver;
	ois_ctrl_t->i2c_client.client = client;
	/* Set device type as I2C */
	ois_ctrl_t->ois_device_type = MSM_CAMERA_I2C_DEVICE;
	ois_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	ois_ctrl_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
	ois_ctrl_t->ois_mutex = &msm_ois_mutex;

	/* Assign name for sub device */
	snprintf(ois_ctrl_t->msm_sd.sd.name, sizeof(ois_ctrl_t->msm_sd.sd.name),
		"%s", ois_ctrl_t->i2c_driver->driver.name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&ois_ctrl_t->msm_sd.sd,
		ois_ctrl_t->i2c_client.client,
		ois_ctrl_t->ois_v4l2_subdev_ops);
	v4l2_set_subdevdata(&ois_ctrl_t->msm_sd.sd, ois_ctrl_t);
	ois_ctrl_t->msm_sd.sd.internal_ops = &msm_ois_internal_ops;
	ois_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&ois_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	ois_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	ois_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_OIS;
	ois_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&ois_ctrl_t->msm_sd);
	ois_ctrl_t->ois_state = OIS_DISABLE_STATE;
	pr_info("msm_ois_i2c_probe: succeeded\n");
	CDBG("Exit\n");

probe_failure:
	kfree(ois_ctrl_t);
	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_ois_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	long rc = 0;
	struct video_device *vdev;
	struct v4l2_subdev *sd;
	struct msm_ois_cfg_data32 *u32;
	struct msm_ois_cfg_data ois_data;
	void *parg;
	struct msm_camera_i2c_seq_reg_setting settings;
	struct msm_camera_i2c_seq_reg_setting32 settings32;

	if (!file || !arg) {
		pr_err("%s:failed NULL parameter\n", __func__);
		return -EINVAL;
	}
	vdev = video_devdata(file);
	sd = vdev_to_v4l2_subdev(vdev);
	u32 = (struct msm_ois_cfg_data32 *)arg;
	parg = arg;

	ois_data.cfgtype = u32->cfgtype;

	switch (cmd) {
	case VIDIOC_MSM_OIS_CFG32:
		cmd = VIDIOC_MSM_OIS_CFG;

		switch (u32->cfgtype) {
		case CFG_OIS_CONTROL:
			ois_data.cfg.set_info.ois_params.setting_size =
				u32->cfg.set_info.ois_params.setting_size;
			ois_data.cfg.set_info.ois_params.i2c_addr =
				u32->cfg.set_info.ois_params.i2c_addr;
			ois_data.cfg.set_info.ois_params.i2c_freq_mode =
				u32->cfg.set_info.ois_params.i2c_freq_mode;
			ois_data.cfg.set_info.ois_params.i2c_addr_type =
				u32->cfg.set_info.ois_params.i2c_addr_type;
			ois_data.cfg.set_info.ois_params.i2c_data_type =
				u32->cfg.set_info.ois_params.i2c_data_type;
			ois_data.cfg.set_info.ois_params.settings =
				compat_ptr(u32->cfg.set_info.ois_params.
				settings);
			parg = &ois_data;
			break;
		case CFG_OIS_I2C_WRITE_SEQ_TABLE:
			if (copy_from_user(&settings32,
				(void *)compat_ptr(u32->cfg.settings),
				sizeof(
				struct msm_camera_i2c_seq_reg_setting32))) {
				pr_err("copy_from_user failed\n");
				return -EFAULT;
			}

			settings.addr_type = settings32.addr_type;
			settings.delay = settings32.delay;
			settings.size = settings32.size;
			settings.reg_setting =
				compat_ptr(settings32.reg_setting);

			ois_data.cfgtype = u32->cfgtype;
			ois_data.cfg.settings = &settings;
			parg = &ois_data;
			break;
		/*ASUS_BSP +++ bill_chen "Implement ois command for dit 3A"*/
		case CFG_OIS_I2C_WRITE_MODE:
			if (copy_from_user(&settings32,
				(void *)compat_ptr(u32->cfg.settings),
				sizeof(
				struct msm_camera_i2c_seq_reg_setting32))) {
				pr_err("copy_from_user failed\n");
				return -EFAULT;
			}

			settings.addr_type = settings32.addr_type;
			settings.delay = settings32.delay;
			settings.size = settings32.size;
			settings.reg_setting =
				compat_ptr(settings32.reg_setting);

			ois_data.cfgtype = u32->cfgtype;
			ois_data.cfg.settings = &settings;
			parg = &ois_data;
			break;
		/*ASUS_BSP --- bill_chen "Implement ois command for dit 3A"*/
		default:
			parg = &ois_data;
			break;
		}
	}
	rc = msm_ois_subdev_ioctl(sd, cmd, parg);

	return rc;
}

static long msm_ois_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_ois_subdev_do_ioctl);
}
#endif

static int32_t msm_ois_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_ois_ctrl_t *msm_ois_t = NULL;
	/*ASUS_BSP +++  bill_chen "Implement ois"*/
	//struct msm_ois_vreg *vreg_cfg;
	uint32_t id_info[3];
	struct msm_camera_power_ctrl_t *power_info = NULL;
	struct msm_ois_board_info *ob_info = NULL;
	pr_info("%s: E", __func__);
	/*ASUS_BSP --- bill_chen "Implement ois"*/
	CDBG("Enter\n");

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	msm_ois_t = kzalloc(sizeof(struct msm_ois_ctrl_t),
		GFP_KERNEL);
	if (!msm_ois_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	msm_ois_t_pointer = msm_ois_t; /*ASUS_BSP bill_chen "Implement ois"*/
	msm_ois_t->oboard_info = kzalloc(sizeof(
		struct msm_ois_board_info), GFP_KERNEL);
	if (!msm_ois_t->oboard_info) {
		kfree(msm_ois_t);
		return -ENOMEM;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		goto release_memory;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
		&msm_ois_t->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_ois_t->cci_master, rc);
	if (rc < 0 || msm_ois_t->cci_master >= MASTER_MAX) {
		pr_err("failed rc %d\n", rc);
		goto release_memory;
	}
	/*ASUS_BSP +++ bill_chen "Implement ois"*/
#if 0
	if (of_find_property((&pdev->dev)->of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &msm_ois_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data((&pdev->dev)->of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			pr_err("failed rc %d\n", rc);
			goto release_memory;
		}
	}

	rc = msm_sensor_driver_get_gpio_data(&(msm_ois_t->gconf),
		(&pdev->dev)->of_node);
	if (rc < 0) {
		pr_err("%s: No/Error OIS GPIO\n", __func__);
	} else {
		msm_ois_t->cam_pinctrl_status = 1;
		rc = msm_camera_pinctrl_init(
			&(msm_ois_t->pinctrl_info), &(pdev->dev));
		if (rc < 0) {
			pr_err("ERR:%s: Error in reading OIS pinctrl\n",
				__func__);
			msm_ois_t->cam_pinctrl_status = 0;
		}
	}
#endif
	/*ASUS_BSP --- bill_chen "Implement ois"*/
	msm_ois_t->ois_mutex = &msm_ois_mutex;

	/* Set platform device handle */
	msm_ois_t->pdev = pdev;
	/* Set device type as platform device */
	msm_ois_t->ois_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_ois_t->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	msm_ois_t->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_ois_t->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_ois_t->i2c_client.cci_client) {
		kfree(msm_ois_t->vreg_cfg.cam_vreg);
		rc = -ENOMEM;
		goto release_memory;
	}

	cci_client = msm_ois_t->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = msm_ois_t->cci_master;


	msm_ois_t->oboard_info = kzalloc(sizeof(
		struct msm_ois_board_info), GFP_KERNEL);
	if (!msm_ois_t->oboard_info) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		rc = -ENOMEM;
	}

	ob_info = msm_ois_t->oboard_info;
	power_info = &ob_info->power_info;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	msm_ois_t->userspace_probe = 0;

	power_info->dev = &pdev->dev;
	msm_ois_t->subdev_id = pdev->id;

	rc = msm_camera_get_clk_info(msm_ois_t->pdev,
		&power_info->clk_info,
		&power_info->clk_ptr,
		&power_info->clk_info_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_clk_info rc %d", rc);
		//goto board_free;
	}

	rc = msm_ois_get_dt_data(msm_ois_t);
	if (rc < 0) {
		pr_err("%s msm_ois_get_dt_data fail rc = %d\n", __func__, rc);
	}

	if (msm_ois_t->userspace_probe == 0) {
		rc = of_property_read_u32_array((&pdev->dev)->of_node, "qcom,slave-id",
			id_info, 3);
		if (rc < 0) {
			pr_err("%s request slave id failed %d rc = %d\n", __func__, __LINE__, rc);
			//goto board_free;
		}

		rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,i2c-freq-mode",
			&cci_client->i2c_freq_mode);
		pr_err("%s: qcom,i2c_freq_mode %d, rc %d\n", __func__, cci_client->i2c_freq_mode, rc);
		if (rc < 0) {
			pr_err("%s qcom,i2c-freq-mode read fail. Setting to 0 %d\n",
				__func__, rc);
			cci_client->i2c_freq_mode = 0;
		}

		msm_ois_t->i2c_client.cci_client->sid = id_info[0] >> 1;
		msm_ois_t->sensor_id_reg_addr = id_info[1];
		msm_ois_t->sensor_id = id_info[2];
		pr_err("%s slave id 0x%x, id reg 0x%x, id val 0x%x\n", __func__,
				msm_ois_t->i2c_client.cci_client->sid,
				msm_ois_t->sensor_id_reg_addr,
				msm_ois_t->sensor_id
				);
		//pr_info"power_info=%p\n",power_info);

		rc = msm_camera_power_up(power_info, msm_ois_t->ois_device_type,
			&msm_ois_t->i2c_client);
		if (rc) {
			pr_err("%s: msm_camera_power_up fail rc = %d\n", __func__, rc);
		}
		else
			pr_err("%s: msm_camera_power_up OK rc = %d\n", __func__, rc);
		delay_ms(130);//wait 100ms for OIS and 30ms for I2C

		rc = msm_ois_check_id(msm_ois_t);
		if(rc < 0)
		{
			pr_err("%s msm_ois_check_id fail %d rc = %d\n", __func__, __LINE__, rc);
			g_ois_status = 0;
			//goto power_down;
		}
		else if(rc == 1)
		{
			pr_info("Maybe FW update failed earlier?\n");
			g_ois_status = 2;//OIS FW Update Failed...
		}
		else
		{
			g_ois_status = 1;
		}
		pr_info("%s g_ois_status = %d\n", __func__, g_ois_status);

		rc = msm_camera_power_down(power_info, msm_ois_t->ois_device_type,
			&msm_ois_t->i2c_client);
		if (rc) {
			pr_err("%s: msm_camera_power_down fail rc = %d\n", __func__, rc);
		}
		else
			pr_info("%s: msm_camera_power_down OK rc = %d\n", __func__, rc);
	}

	asus_ois_init(msm_ois_t_pointer);

	if(g_ois_status != 0)
	{
		msm_ois_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
		v4l2_subdev_init(&msm_ois_t->msm_sd.sd,
			msm_ois_t->ois_v4l2_subdev_ops);
		v4l2_set_subdevdata(&msm_ois_t->msm_sd.sd, msm_ois_t);
		msm_ois_t->msm_sd.sd.internal_ops = &msm_ois_internal_ops;
		msm_ois_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
		snprintf(msm_ois_t->msm_sd.sd.name,
			ARRAY_SIZE(msm_ois_t->msm_sd.sd.name), "msm_ois");
		media_entity_init(&msm_ois_t->msm_sd.sd.entity, 0, NULL, 0);
		msm_ois_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
		msm_ois_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_OIS;
		msm_ois_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
		rc = msm_sd_register(&msm_ois_t->msm_sd);
		if(rc < 0)
			pr_err("msm_sd_register ois failed!\n");
		msm_ois_t->ois_state = OIS_DISABLE_STATE;
		msm_cam_copy_v4l2_subdev_fops(&msm_ois_v4l2_subdev_fops);
	#ifdef CONFIG_COMPAT
		msm_ois_v4l2_subdev_fops.compat_ioctl32 =
			msm_ois_subdev_fops_ioctl;
	#endif
		msm_ois_t->msm_sd.sd.devnode->fops =
			&msm_ois_v4l2_subdev_fops;
	}
    else
    {
		rc = -1;
		pr_err("probe failed, not create v4l2 node\n");
	}
	pr_info("%s: rc = %d X", __func__, rc);
	return rc;
release_memory:
	kfree(msm_ois_t->oboard_info);
	kfree(msm_ois_t);
	return rc;
}

static const struct of_device_id msm_ois_i2c_dt_match[] = {
	{.compatible = "qcom,ois"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_ois_i2c_dt_match);

static struct i2c_driver msm_ois_i2c_driver = {
	.id_table = msm_ois_i2c_id,
	.probe  = msm_ois_i2c_probe,
	.remove = __exit_p(msm_ois_i2c_remove),
	.driver = {
		.name = "qcom,ois",
		.owner = THIS_MODULE,
		.of_match_table = msm_ois_i2c_dt_match,
	},
};

static const struct of_device_id msm_ois_dt_match[] = {
	{.compatible = "qcom,rumba-s4a", .data = NULL},
	{}
};

MODULE_DEVICE_TABLE(of, msm_ois_dt_match);

static struct platform_driver msm_ois_platform_driver = {
	.probe = msm_ois_platform_probe,
	.driver = {
		.name = "qcom,rumba-s4a",
		.owner = THIS_MODULE,
		.of_match_table = msm_ois_dt_match,
	},
};

static int __init msm_ois_init_module(void)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	rc = platform_driver_register(&msm_ois_platform_driver);
	if (!rc)
		return rc;
	CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&msm_ois_i2c_driver);
}

static void __exit msm_ois_exit_module(void)
{
	platform_driver_unregister(&msm_ois_platform_driver);
	i2c_del_driver(&msm_ois_i2c_driver);
	return;
}

static int32_t msm_ois_check_id(struct msm_ois_ctrl_t *o_ctrl) {

	int32_t rc = 0;
	uint16_t chipid = 0;
	uint32_t fw_version = 0;
	uint16_t hw_id = 0;
	int is_fw_bad = 0;
	rc = rumba_read_byte(o_ctrl,o_ctrl->sensor_id_reg_addr,&chipid);

    pr_info("read id: 0x%x expected: 0x%x\n",chipid, o_ctrl->sensor_id);

    if(rc < 0) {
        pr_err("ois read i2c id fail!");
        rc = -EFAULT;
		goto OIS_CHECK_FAIL;
    }

    if(chipid != o_ctrl->sensor_id)
    {
        pr_err("ois sensor id not match!\n");
        if( chipid == 0x80 )
        {
			pr_info("FW update failed earlier? for chipid == 0x80\n");
			is_fw_bad = 1;
		}
        else
        {
			rc = -EFAULT;
			goto OIS_CHECK_FAIL;
		}
    }

	rc = rumba_read_dword(o_ctrl,0x00FC,&fw_version);
	if(rc == 0)
	{
		pr_info("FW revision 0x%4X %d\n",fw_version,fw_version);
		g_ois_reg_fw_version = fw_version;
		rc = rumba_read_word(o_ctrl,0x00F8,&hw_id);
		if(rc == 0)
			pr_info("HW ID 0x%4X\n",hw_id);
	}
	if(is_fw_bad)
		rc = 1;
OIS_CHECK_FAIL:
    pr_err("%s: rc = %d X\n", __func__, rc);
    return rc;
}


static int msm_ois_get_dt_data(struct msm_ois_ctrl_t *o_ctrl)
{
	int rc = 0, i = 0;
	struct msm_ois_board_info *ob_info;
	struct msm_camera_power_ctrl_t *power_info =
		&o_ctrl->oboard_info->power_info;
	struct device_node *of_node = NULL;
	struct msm_camera_gpio_conf *gconf = NULL;
	int8_t gpio_array_size = 0;
	uint16_t *gpio_array = NULL;

	ob_info = o_ctrl->oboard_info;

	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		of_node = o_ctrl->pdev->dev.of_node;

	if (!of_node) {
		pr_err("%s: %d of_node is NULL\n", __func__ , __LINE__);
		return -ENOMEM;
	}

	rc = msm_camera_get_dt_vreg_data(of_node, &power_info->cam_vreg,
					     &power_info->num_vreg);
	pr_info("%s power_info->num_vreg = %d\n", __func__, power_info->num_vreg);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}

	if (o_ctrl->userspace_probe == 0) {
		rc = msm_camera_get_dt_power_setting_data(of_node,
			power_info->cam_vreg, power_info->num_vreg,
			power_info);
		if (rc < 0)
			goto ERROR1;
		else
			pr_info("%s get power setting dt data OK\n",__func__);

		//fix non-regulator gpio setting in power down
		fix_gpio_value_of_power_down_setting(power_info);

	}

	power_info->gpio_conf = kzalloc(sizeof(struct msm_camera_gpio_conf),
					GFP_KERNEL);
	if (!power_info->gpio_conf) {
		rc = -ENOMEM;
		goto ERROR2;
	}

	gconf = power_info->gpio_conf;
	gpio_array_size = of_gpio_count(of_node);
	pr_info("gpio count %d\n", gpio_array_size);

	if (gpio_array_size > 0) {
		gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size,
			GFP_KERNEL);
		if (!gpio_array) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR3;
		}
		for (i = 0; i < gpio_array_size; i++) {
			gpio_array[i] = of_get_gpio(of_node, i);
			pr_info("%s gpio_array[%d] = %d\n", __func__, i,
				gpio_array[i]);
		}

		rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR4;
		}

		rc = msm_camera_init_gpio_pin_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR4;
		}

		kfree(gpio_array);
	}

	return rc;
ERROR4:
	kfree(gpio_array);
ERROR3:
	kfree(power_info->gpio_conf);
ERROR2:
	kfree(power_info->cam_vreg);
ERROR1:
	kfree(power_info->power_setting);
	return rc;
}

late_initcall(msm_ois_init_module);
module_exit(msm_ois_exit_module);
MODULE_DESCRIPTION("MSM OIS");
MODULE_LICENSE("GPL v2");