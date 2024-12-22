#ifndef __LINUX_MSM_CAM_SENSOR_H
#define __LINUX_MSM_CAM_SENSOR_H

#ifdef MSM_CAMERA_BIONIC
#include <sys/types.h>
#endif

#include <media/msm_camsensor_sdk.h>

#include <linux/types.h>
#include <linux/i2c.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define I2C_SEQ_REG_SETTING_MAX   5

#define MSM_SENSOR_MCLK_8HZ   8000000
#define MSM_SENSOR_MCLK_16HZ  16000000
#define MSM_SENSOR_MCLK_24HZ  24000000

#define MAX_SENSOR_NAME 32
#define MAX_ACTUATOR_AF_TOTAL_STEPS 1024

#define MOVE_NEAR 0
#define MOVE_FAR  1

#define MSM_ACTUATOR_MOVE_SIGNED_FAR -1
#define MSM_ACTUATOR_MOVE_SIGNED_NEAR  1

#define MAX_AF_ITERATIONS 3
#define MAX_NUMBER_OF_STEPS 47

#define MAX_LED_TRIGGERS        3
#define MSM_OIS_VER_SIZE       (6)


//************************************* Native functionalities for YUV sensor added
#define EXT_CAM_EV			1
#define EXT_CAM_WB			2
#define EXT_CAM_METERING	3
#define EXT_CAM_ISO			4
#define EXT_CAM_EFFECT		5
#define EXT_CAM_SCENE_MODE	6
#define EXT_CAM_SENSOR_MODE	7
#define EXT_CAM_CONTRAST	8
#define EXT_CAM_EXIF		9
#define EXT_CAM_SET_AE_AWB	10
//Focus related enums
#define EXT_CAM_AF              11
#define EXT_CAM_FOCUS           12
#define EXT_CAM_SET_TOUCHAF_POS	13
#define EXT_CAM_SET_AF_STATUS	14
#define EXT_CAM_GET_AF_STATUS	15
#define EXT_CAM_GET_AF_RESULT	16
#define EXT_CAM_SET_AF_STOP     17
#define EXT_CAM_FLASH_MODE      18
#define EXT_CAM_SET_FLASH       19
#define EXT_CAM_VT_MODE         20
#define EXT_CAM_FPS_RANGE       21

//Exposure Compensation
#define CAMERA_EV_M4		0
#define CAMERA_EV_M3		1
#define CAMERA_EV_M2		2
#define CAMERA_EV_M1		3
#define CAMERA_EV_DEFAULT	4
#define CAMERA_EV_P1		5
#define CAMERA_EV_P2		6
#define CAMERA_EV_P3		7
#define CAMERA_EV_P4		8

//White Balance
#define CAMERA_WHITE_BALANCE_OFF				0
#define CAMERA_WHITE_BALANCE_AUTO				1
#define CAMERA_WHITE_BALANCE_INCANDESCENT		2
#define CAMERA_WHITE_BALANCE_FLUORESCENT		3
#define CAMERA_WHITE_BALANCE_DAYLIGHT			5
#define CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT	6

//Metering
#define CAMERA_AVERAGE			0
#define CAMERA_CENTER_WEIGHT	1
#define CAMERA_SPOT				2

//ISO
#define CAMERA_ISO_MODE_AUTO	0
#define CAMERA_ISO_MODE_50		1
#define CAMERA_ISO_MODE_100		2
#define CAMERA_ISO_MODE_200		3
#define CAMERA_ISO_MODE_400		4
#define CAMERA_ISO_MODE_800		5

//Effect
#define CAMERA_EFFECT_OFF			0
#define CAMERA_EFFECT_MONO			1
#define CAMERA_EFFECT_NEGATIVE		2
#define CAMERA_EFFECT_SOLARIZE		3
#define CAMERA_EFFECT_SEPIA			4
#define CAMERA_EFFECT_POSTERIZE		5
#define CAMERA_EFFECT_WHITEBOARD	6
#define CAMERA_EFFECT_BLACKBOARD	7
#define CAMERA_EFFECT_AQUA			8
#define CAMERA_EFFECT_EMBOSS		9
#define CAMERA_EFFECT_SKETCH		10
#define CAMERA_EFFECT_NEON			11
#if defined(CONFIG_MACH_GT5NOTE10_KOR_OPEN)
#define CAMERA_EFFECT_VINTAGE_WARM     12
#define CAMERA_EFFECT_VINTAGE_COOL     13
#define CAMERA_EFFECT_BLUE             14
#define CAMERA_EFFECT_ORANGE           15
#define CAMERA_EFFECT_GREEN            16
#define CAMERA_EFFECT_BEAUTY           17
#define CAMERA_EFFECT_MAX              18
#else
#define CAMERA_EFFECT_WASHED		12
#define CAMERA_EFFECT_VINTAGE_WARM	13
#define CAMERA_EFFECT_VINTAGE_COLD	14
#define CAMERA_EFFECT_POINT_COLOR_1	15
#define CAMERA_EFFECT_POINT_COLOR_2	16
#define CAMERA_EFFECT_POINT_COLOR_3	17
#define CAMERA_EFFECT_POINT_COLOR_4	18
#define CAMERA_EFFECT_CARTOONIZE	19
#define CAMERA_EFFECT_MAX			20
#endif

//scene mode
#define CAMERA_SCENE_AUTO           0
#define CAMERA_SCENE_LANDSCAPE      3
#define CAMERA_SCENE_NIGHT          6
#define CAMERA_SCENE_BEACH          7
#define CAMERA_SCENE_PORTRAIT       8
#define CAMERA_SCENE_SPORT          9
#define CAMERA_SCENE_FIRE           11
#define CAMERA_SCENE_SUNSET         12
#define CAMERA_SCENE_PARTY          13
#define CAMERA_SCENE_CANDLE         14
#define CAMERA_SCENE_AGAINST_LIGHT  16
#define CAMERA_SCENE_FALL           17
#define CAMERA_SCENE_TEXT           19
#define CAMERA_SCENE_DAWN           21	//need to check


#define CAMERA_CONTRAST_LV0			0
#define CAMERA_CONTRAST_LV1			1
#define CAMERA_CONTRAST_LV2			2
#define CAMERA_CONTRAST_LV3			3
#define CAMERA_CONTRAST_LV4			4
#define CAMERA_CONTRAST_LV5			5
#define CAMERA_CONTRAST_LV6			6
#define CAMERA_CONTRAST_LV7			7
#define CAMERA_CONTRAST_LV8			8
#define CAMERA_CONTRAST_LV9			9

#define CAMERA_MODE_INIT                0
#define CAMERA_MODE_PREVIEW             1
#define CAMERA_MODE_CAPTURE             2
#define CAMERA_MODE_RECORDING           3

//Flash modes
#define CAMERA_FLASH_OFF        0
#define CAMERA_FLASH_ON         2
#define CAMERA_FLASH_AUTO       1
#define CAMERA_FLASH_TORCH      3 

//AF modes
#define CAMERA_AF_AUTO          0
#define CAMERA_AF_MACRO         2
#define CAMERA_AF_OCR           3

enum flash_type {
#if !defined(CONFIG_FLED_SM5701)
	LED_FLASH = 1,
#endif
	STROBE_FLASH,
	GPIO_FLASH
};

enum msm_sensor_resolution_t {
	MSM_SENSOR_RES_FULL,
	MSM_SENSOR_RES_QTR,
	MSM_SENSOR_RES_2,
	MSM_SENSOR_RES_3,
	MSM_SENSOR_RES_4,
	MSM_SENSOR_RES_5,
	MSM_SENSOR_RES_6,
	MSM_SENSOR_RES_7,
	MSM_SENSOR_RES_8,
	MSM_SENSOR_RES_9,
	MSM_SENSOR_RES_10,
	MSM_SENSOR_RES_11,
	MSM_SENSOR_RES_12,
	MSM_SENSOR_RES_13,
	MSM_SENSOR_INVALID_RES,
};

enum msm_camera_stream_type_t {
	MSM_CAMERA_STREAM_PREVIEW,
	MSM_CAMERA_STREAM_SNAPSHOT,
	MSM_CAMERA_STREAM_VIDEO,
	MSM_CAMERA_STREAM_INVALID,
};

enum {
	MSM_CAMERA_EFFECT_MODE_OFF,
	MSM_CAMERA_EFFECT_MODE_MONO,
	MSM_CAMERA_EFFECT_MODE_NEGATIVE,
	MSM_CAMERA_EFFECT_MODE_SOLARIZE,
	MSM_CAMERA_EFFECT_MODE_SEPIA,
	MSM_CAMERA_EFFECT_MODE_POSTERIZE,
	MSM_CAMERA_EFFECT_MODE_WHITEBOARD,
	MSM_CAMERA_EFFECT_MODE_BLACKBOARD,
	MSM_CAMERA_EFFECT_MODE_AQUA,
	MSM_CAMERA_EFFECT_MODE_EMBOSS,
	MSM_CAMERA_EFFECT_MODE_SKETCH,
	MSM_CAMERA_EFFECT_MODE_NEON,
	MSM_CAMERA_EFFECT_MODE_MAX
};

enum {
	MSM_CAMERA_WB_MODE_AUTO,
	MSM_CAMERA_WB_MODE_CUSTOM,
	MSM_CAMERA_WB_MODE_INCANDESCENT,
	MSM_CAMERA_WB_MODE_FLUORESCENT,
	MSM_CAMERA_WB_MODE_WARM_FLUORESCENT,
	MSM_CAMERA_WB_MODE_DAYLIGHT,
	MSM_CAMERA_WB_MODE_CLOUDY_DAYLIGHT,
	MSM_CAMERA_WB_MODE_TWILIGHT,
	MSM_CAMERA_WB_MODE_SHADE,
	MSM_CAMERA_WB_MODE_OFF,
	MSM_CAMERA_WB_MODE_MAX
};

enum {
	MSM_CAMERA_SCENE_MODE_OFF,
	MSM_CAMERA_SCENE_MODE_AUTO,
	MSM_CAMERA_SCENE_MODE_LANDSCAPE,
	MSM_CAMERA_SCENE_MODE_SNOW,
	MSM_CAMERA_SCENE_MODE_BEACH,
	MSM_CAMERA_SCENE_MODE_SUNSET,
	MSM_CAMERA_SCENE_MODE_NIGHT,
	MSM_CAMERA_SCENE_MODE_PORTRAIT,
	MSM_CAMERA_SCENE_MODE_BACKLIGHT,
	MSM_CAMERA_SCENE_MODE_SPORTS,
	MSM_CAMERA_SCENE_MODE_ANTISHAKE,
	MSM_CAMERA_SCENE_MODE_FLOWERS,
	MSM_CAMERA_SCENE_MODE_CANDLELIGHT,
	MSM_CAMERA_SCENE_MODE_FIREWORKS,
	MSM_CAMERA_SCENE_MODE_PARTY,
	MSM_CAMERA_SCENE_MODE_NIGHT_PORTRAIT,
	MSM_CAMERA_SCENE_MODE_THEATRE,
	MSM_CAMERA_SCENE_MODE_ACTION,
	MSM_CAMERA_SCENE_MODE_AR,
	MSM_CAMERA_SCENE_MODE_FACE_PRIORITY,
	MSM_CAMERA_SCENE_MODE_BARCODE,
	MSM_CAMERA_SCENE_MODE_HDR,
	MSM_CAMERA_SCENE_MODE_MAX
};

enum csid_cfg_type_t {
	CSID_INIT,
	CSID_CFG,
	CSID_RELEASE,
	CSID_BOOSTOFF,
};

enum csiphy_cfg_type_t {
	CSIPHY_INIT,
	CSIPHY_CFG,
	CSIPHY_RELEASE,
};

enum camera_vreg_type {
	REG_LDO,
	REG_VS,
	REG_GPIO,
};

enum sensor_af_e {
    SENSOR_AF_CANCEL = 1,
    SENSOR_AF_START,
    SENSOR_AF_PRE_FLASH_ON,
    SENSOR_AF_PRE_FLASH_OFF,
    SENSOR_AF_PRE_FLASH_AE_STABLE,
};

struct msm_sensor_gpio_config {
enum msm_sensor_power_seq_gpio_t gpio_name;
uint8_t config_val;
};

enum sensor_af_t {
	SENSOR_AF_FOCUSSED,
	SENSOR_AF_NOT_FOCUSSED,
};

enum cci_i2c_master_t {
	MASTER_0,
	MASTER_1,
	MASTER_MAX,
};

struct msm_camera_i2c_array_write_config {
	struct msm_camera_i2c_reg_setting conf_array;
	uint16_t slave_addr;
};

struct msm_camera_i2c_read_config {
	uint16_t slave_addr;
	uint16_t reg_addr;
	enum msm_camera_i2c_data_type data_type;
	uint16_t data;
};

struct msm_camera_csi2_params {
	struct msm_camera_csid_params csid_params;
	struct msm_camera_csiphy_params csiphy_params;
	uint8_t csi_clk_scale_enable;
};

struct msm_camera_csi_lane_params {
	uint16_t csi_lane_assign;
	uint16_t csi_lane_mask;
};

struct csi_lane_params_t {
	uint16_t csi_lane_assign;
	uint8_t csi_lane_mask;
	uint8_t csi_if;
	int8_t csid_core[2];
	uint8_t csi_phy_sel;
};

struct camera_vreg_t {
	const char *reg_name;
	enum camera_vreg_type type;
	int min_voltage;
	int max_voltage;
	int op_mode;
	uint32_t delay;
#if defined(CONFIG_CAM_DUAL_POWER_SEQ)
	void *regulator[1];
#endif
};

struct sensorb_cfg_data {
    int cfgtype;
    int flicker_type;
    union {
		struct msm_sensor_info_t      sensor_info;
		struct msm_sensor_init_params sensor_init_params;
		void                         *setting;
	} cfg;
};

struct csid_cfg_data {
	enum csid_cfg_type_t cfgtype;
	union {
		uint32_t csid_version;
		struct msm_camera_csid_params *csid_params;
	} cfg;
};

struct csiphy_cfg_data {
	enum csiphy_cfg_type_t cfgtype;
	union {
		struct msm_camera_csiphy_params *csiphy_params;
		struct msm_camera_csi_lane_params *csi_lane_params;
	} cfg;
};

struct msm_ois_cal_info_t {
	char  cal_ver[MSM_OIS_VER_SIZE+1];
	uint16_t  cal_checksum_rumba;
	uint16_t  cal_checksum_line;
	uint8_t  is_different_crc;
};
struct msm_ois_cfg_data {
	int cfgtype;
	uint16_t reg_addr;
	uint16_t read_data;
	uint16_t write_data;
	uint16_t set_mode_value;
	uint8_t  *version;
	struct msm_ois_cal_info_t *ois_cal_info;
};

enum eeprom_cfg_type_t {
	CFG_EEPROM_GET_INFO,
	CFG_EEPROM_GET_CAL_DATA,
	CFG_EEPROM_READ_CAL_DATA,
	CFG_EEPROM_READ_DATA,
	CFG_EEPROM_READ_COMPRESSED_DATA,
	CFG_EEPROM_WRITE_DATA,
	CFG_EEPROM_GET_ERASESIZE,
	CFG_EEPROM_ERASE,
	CFG_EEPROM_POWER_ON,
	CFG_EEPROM_POWER_OFF,
	CFG_EEPROM_READ_DATA_FROM_HW,
	CFG_EEPROM_GET_MM_INFO,
	CFG_EEPROM_GET_FW_VERSION_INFO,
};

struct eeprom_get_t {
	uint32_t num_bytes;
};

struct eeprom_read_t {
	uint8_t *dbuffer;
	uint32_t num_bytes;
	uint32_t addr;
	uint32_t comp_size;
};

struct eeprom_write_t {
	uint8_t *dbuffer;
	uint32_t num_bytes;
	uint32_t addr;
	uint32_t *write_size;
	uint8_t compress;
};

struct eeprom_erase_t {
	uint32_t num_bytes;
	uint32_t addr;
};

struct eeprom_get_cmm_t {
	uint32_t cmm_support;
	uint32_t cmm_compression;
	uint32_t cmm_size;
};

struct msm_eeprom_cfg_data {
	enum eeprom_cfg_type_t cfgtype;
	uint8_t is_supported;
	union {
		char eeprom_name[MAX_EEPROM_NAME];
		struct eeprom_get_t get_data;
		struct eeprom_read_t read_data;
		struct eeprom_write_t write_data;
		struct eeprom_erase_t erase_data;
		struct eeprom_get_cmm_t get_cmm_data;
	} cfg;
};

#ifdef CONFIG_COMPAT
struct msm_sensor_power_setting32 {
	enum msm_sensor_power_seq_type_t seq_type;
	uint16_t seq_val;
	compat_uint_t config_val;
	uint16_t delay;
	compat_uptr_t data[10];
};

struct msm_sensor_power_setting_array32 {
	compat_uptr_t power_setting;
	uint16_t size;
	compat_uptr_t power_down_setting;
	uint16_t size_down;
};

struct msm_camera_sensor_slave_info32 {
	char sensor_name[32];
	char eeprom_name[32];
	char actuator_name[32];
	enum msm_sensor_camera_id_t camera_id;
	uint16_t slave_addr;
	enum i2c_freq_mode_t i2c_freq_mode;
	enum msm_camera_i2c_reg_addr_type addr_type;
	enum msm_camera_i2c_data_type data_type;
	struct msm_sensor_id_info_t sensor_id_info;
	struct msm_sensor_power_setting_array32 power_setting_array;
	uint8_t  is_init_params_valid;
	struct msm_sensor_init_params sensor_init_params;
	uint8_t is_probe_succeed;
	char subdev_name[32];
	struct msm_sensor_info_t sensor_info;
};

struct msm_camera_csid_lut_params32 {
	uint8_t num_cid;
	compat_uptr_t vc_cfg[MAX_CID];
};

struct msm_camera_csid_params32 {
	uint8_t lane_cnt;
	uint16_t lane_assign;
	uint8_t phy_sel;
	struct msm_camera_csid_lut_params32 lut_params;
};

struct msm_camera_csi2_params32 {
	struct msm_camera_csid_params32 csid_params;
	struct msm_camera_csiphy_params csiphy_params;
	uint8_t csi_clk_scale_enable;
};

struct csid_cfg_data32 {
	enum csid_cfg_type_t cfgtype;
	union {
		uint32_t csid_version;
		compat_uptr_t csid_params;
	} cfg;
};

struct eeprom_read_t32 {
	compat_uptr_t dbuffer;
	uint32_t num_bytes;
	uint32_t addr;
	uint32_t comp_size;
};

struct eeprom_write_t32 {
	compat_uptr_t dbuffer;
	uint32_t num_bytes;
	uint32_t addr;
	compat_uptr_t write_size;
	uint8_t compress;
};

struct msm_eeprom_cfg_data32 {
	enum eeprom_cfg_type_t cfgtype;
	uint8_t is_supported;
	union {
		char eeprom_name[MAX_EEPROM_NAME];
		struct eeprom_get_t get_data;
		struct eeprom_read_t32 read_data;
		struct eeprom_write_t32 write_data;
		struct eeprom_erase_t erase_data;
		struct eeprom_get_cmm_t get_cmm_data;
	} cfg;
};

struct msm_camera_i2c_seq_reg_setting32 {
	compat_uptr_t reg_setting;
	uint16_t size;
	enum msm_camera_i2c_reg_addr_type addr_type;
	uint16_t delay;
};
#endif

enum msm_sensor_cfg_type_t {
	CFG_SET_SLAVE_INFO,
	CFG_SLAVE_READ_I2C,
	CFG_WRITE_I2C_ARRAY,
	CFG_SLAVE_WRITE_I2C_ARRAY,
	CFG_WRITE_I2C_SEQ_ARRAY,
	CFG_POWER_UP,
	CFG_POWER_DOWN,
	CFG_SET_STOP_STREAM_SETTING,
	CFG_GET_SENSOR_INFO,
	CFG_GET_SENSOR_INIT_PARAMS,
	CFG_SET_INIT_SETTING,
	CFG_SET_RESOLUTION,
	CFG_SET_STOP_STREAM,
	CFG_SET_START_STREAM,
	CFG_SET_GPIO_STATE,
	CFG_SET_SENSOR_OTP_CAL, // Randy 10.08
	CFG_SET_SATURATION,
	CFG_SET_CONTRAST,
	CFG_SET_SHARPNESS,
	CFG_SET_ISO,
	CFG_SET_EXPOSURE_COMPENSATION,
	CFG_SET_ANTIBANDING,
	CFG_SET_BESTSHOT_MODE,
	CFG_SET_EFFECT,
	CFG_SET_WHITE_BALANCE,
	CFG_SET_AUTOFOCUS,
	CFG_CANCEL_AUTOFOCUS,
	CFG_SET_STREAM_TYPE,
};

enum msm_ois_cfg_type_t {
	CFG_OIS_POWERDOWN,
	CFG_OIS_POWERUP,
	CFG_OIS_SET_MODE,
	CFG_OIS_READ_MODULE_VER,
	CFG_OIS_READ_PHONE_VER,
	CFG_OIS_READ_CAL_INFO,
	CFG_OIS_GET_FW_STATUS,
	CFG_OIS_FW_UPDATE,
};

enum msm_actuator_cfg_type_t {
	CFG_GET_ACTUATOR_INFO,
	CFG_SET_ACTUATOR_INFO,
	CFG_SET_DEFAULT_FOCUS,
	CFG_MOVE_FOCUS,
	CFG_SET_POSITION,
	CFG_ACTUATOR_POWERDOWN,
	CFG_ACTUATOR_POWERUP,
	CFG_SET_ACTUATOR_SW_LANDING,
	CFG_ACTUATOR_INIT,
};

struct msm_actuator_move_params_t {
	int8_t dir;
	int8_t sign_dir;
	int16_t dest_step_pos;
	int32_t num_steps;
	uint16_t curr_lens_pos;
	struct damping_params_t *ringing_params;
};

struct msm_actuator_tuning_params_t {
	int16_t initial_code;
	uint16_t pwd_step;
	uint16_t region_size;
	uint32_t total_steps;
	struct region_params_t *region_params;
};

struct msm_actuator_params_t {
	enum actuator_type act_type;
	uint8_t reg_tbl_size;
	uint16_t data_size;
	uint16_t init_setting_size;
	uint32_t i2c_addr;
	enum msm_actuator_addr_type i2c_addr_type;
	enum msm_actuator_data_type i2c_data_type;
	struct msm_actuator_reg_params_t *reg_tbl_params;
	struct reg_settings_t *init_settings;
};

struct msm_actuator_set_info_t {
	struct msm_actuator_params_t actuator_params;
	struct msm_actuator_tuning_params_t af_tuning_params;
};

struct msm_actuator_get_info_t {
	uint32_t focal_length_num;
	uint32_t focal_length_den;
	uint32_t f_number_num;
	uint32_t f_number_den;
	uint32_t f_pix_num;
	uint32_t f_pix_den;
	uint32_t total_f_dist_num;
	uint32_t total_f_dist_den;
	uint32_t hor_view_angle_num;
	uint32_t hor_view_angle_den;
	uint32_t ver_view_angle_num;
	uint32_t ver_view_angle_den;
};

enum af_camera_name {
	ACTUATOR_MAIN_CAM_0,
	ACTUATOR_MAIN_CAM_1,
	ACTUATOR_MAIN_CAM_2,
	ACTUATOR_MAIN_CAM_3,
	ACTUATOR_MAIN_CAM_4,
	ACTUATOR_MAIN_CAM_5,
	ACTUATOR_WEB_CAM_0,
	ACTUATOR_WEB_CAM_1,
	ACTUATOR_WEB_CAM_2,
};


struct msm_actuator_set_position_t {
	uint16_t number_of_steps;
	uint16_t pos[MAX_NUMBER_OF_STEPS];
	uint16_t delay[MAX_NUMBER_OF_STEPS];
};

struct msm_actuator_cfg_data {
	int cfgtype;
	int sw_landing_type;
	uint8_t is_af_supported;
	union {
		struct msm_actuator_move_params_t move;
		struct msm_actuator_set_info_t set_info;
		struct msm_actuator_get_info_t get_info;
		struct msm_actuator_set_position_t setpos;
		enum af_camera_name cam_name;
	} cfg;
};

enum msm_camera_led_config_t {
	MSM_CAMERA_LED_OFF,
	MSM_CAMERA_LED_LOW,
	MSM_CAMERA_LED_HIGH,
	MSM_CAMERA_LED_INIT,
	MSM_CAMERA_LED_RELEASE,
	MSM_CAMERA_LED_PREFLASH,
};

struct msm_camera_led_cfg_t {
	enum msm_camera_led_config_t cfgtype;
	uint32_t torch_current;
	uint32_t flash_current[MAX_LED_TRIGGERS];
};

/* sensor init structures and enums */
enum msm_sensor_init_cfg_type_t {
	CFG_SINIT_PROBE,
	CFG_SINIT_PROBE_DONE,
	CFG_SINIT_PROBE_WAIT_DONE,
};

struct sensor_init_cfg_data {
	enum msm_sensor_init_cfg_type_t cfgtype;
	union {
		void *setting;
	} cfg;
};

struct ioctl_native_cmd {
        unsigned short mode;
        unsigned short address;
        unsigned short value_1;
        unsigned short value_2;
        unsigned short value_3;
};

typedef struct
{
	unsigned short iso;
	unsigned short shutterspeed;
	unsigned short isFlashOn;
} exif_data_t;

enum msm_cam_flicker_type {
	MSM_CAM_FLICKER_50HZ,
	MSM_CAM_FLICKER_60HZ,
};

#define VIDIOC_MSM_SENSOR_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct sensorb_cfg_data)

#define VIDIOC_MSM_SENSOR_RELEASE \
	_IO('V', BASE_VIDIOC_PRIVATE + 2)

#define VIDIOC_MSM_SENSOR_GET_SUBDEV_ID \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 3, uint32_t)

#define VIDIOC_MSM_CSIPHY_IO_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct csiphy_cfg_data)

#define VIDIOC_MSM_CSID_IO_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct csid_cfg_data)

#ifdef CONFIG_COMPAT
#define VIDIOC_MSM_CSID_IO_CFG32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct csid_cfg_data32)
#endif

#define VIDIOC_MSM_ACTUATOR_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 6, struct msm_actuator_cfg_data)

#define VIDIOC_MSM_FLASH_LED_DATA_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 7, struct msm_camera_led_cfg_t)

#define VIDIOC_MSM_EEPROM_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 8, struct msm_eeprom_cfg_data)

#ifdef CONFIG_COMPAT
#define VIDIOC_MSM_EEPROM_CFG32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 8, struct msm_eeprom_cfg_data32)
#endif

#define VIDIOC_MSM_SENSOR_GET_AF_STATUS \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, uint32_t)

#define VIDIOC_MSM_SENSOR_INIT_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 10, struct sensor_init_cfg_data)

#define VIDIOC_MSM_SENSOR_NATIVE_CMD \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 11, struct ioctl_native_cmd)

#define VIDIOC_MSM_OIS_IO_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 12, struct msm_ois_cfg_data)

#ifdef CONFIG_COMPAT
struct msm_camera_i2c_reg_setting32 {
	compat_uptr_t reg_setting;
	uint16_t size;
	enum msm_camera_i2c_reg_addr_type addr_type;
	enum msm_camera_i2c_data_type data_type;
	uint16_t delay;
};

struct msm_camera_i2c_array_write_config32 {
	struct msm_camera_i2c_reg_setting32 conf_array;
	uint16_t slave_addr;
};

struct msm_actuator_tuning_params_t32 {
	int16_t initial_code;
	uint16_t pwd_step;
	uint16_t region_size;
	uint32_t total_steps;
	compat_uptr_t region_params;
};

struct msm_actuator_params_t32 {
	enum actuator_type act_type;
	uint8_t reg_tbl_size;
	uint16_t data_size;
	uint16_t init_setting_size;
	uint32_t i2c_addr;
	enum msm_actuator_addr_type i2c_addr_type;
	enum msm_actuator_data_type i2c_data_type;
	compat_uptr_t reg_tbl_params;
	compat_uptr_t init_settings;
};

struct msm_actuator_set_info_t32 {
	struct msm_actuator_params_t32 actuator_params;
	struct msm_actuator_tuning_params_t32 af_tuning_params;
};

struct sensor_init_cfg_data32 {
	enum msm_sensor_init_cfg_type_t cfgtype;
	union {
		compat_uptr_t setting;
	} cfg;
};

struct msm_actuator_move_params_t32 {
	int8_t dir;
	int8_t sign_dir;
	int16_t dest_step_pos;
	int32_t num_steps;
	uint16_t curr_lens_pos;
	compat_uptr_t ringing_params;
};

struct msm_actuator_cfg_data32 {
	int cfgtype;
	int sw_landing_type;
	uint8_t is_af_supported;
	union {
		struct msm_actuator_move_params_t32 move;
		struct msm_actuator_set_info_t32 set_info;
		struct msm_actuator_get_info_t get_info;
		struct msm_actuator_set_position_t setpos;
		enum af_camera_name cam_name;
	} cfg;
};

struct csiphy_cfg_data32 {
	enum csiphy_cfg_type_t cfgtype;
	union {
		compat_uptr_t csiphy_params;
		compat_uptr_t csi_lane_params;
	} cfg;
};

struct sensorb_cfg_data32 {
	int cfgtype;
	int flicker_type;
	union {
		struct msm_sensor_info_t      sensor_info;
		struct msm_sensor_init_params sensor_init_params;
		compat_uptr_t                 setting;
	} cfg;
};

#define VIDIOC_MSM_ACTUATOR_CFG32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 6, struct msm_actuator_cfg_data32)

#define VIDIOC_MSM_SENSOR_INIT_CFG32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 10, struct sensor_init_cfg_data32)

#define VIDIOC_MSM_CSIPHY_IO_CFG32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct csiphy_cfg_data32)

#define VIDIOC_MSM_SENSOR_CFG32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct sensorb_cfg_data32)
#endif

#endif /* __LINUX_MSM_CAM_SENSOR_H */
