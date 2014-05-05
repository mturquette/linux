/*
 * A V4L2 driver for Aptina MT9M114 cameras.
 *
 * Copyright 2011 Aldebaran Robotics  Written
 * by Joseph Pinkasfeld with substantial inspiration from ov7670 code.
 * 
 *  Authors:
 *  joseph pinkasfeld <joseph.pinkasfeld@gmail.com>
 *  Ludovic SMAL <lsmal@aldebaran-robotics.com>
 *  Corentin Le Molgat <clemolgat@aldebaran-robotics.com>
 *  Arne B�ckmann <arneboe@tzi.de>
 *
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>

MODULE_AUTHOR("Joseph Pinkasfeld <joseph.pinkasfeld@gmail.com>;Ludovic SMAL <lsmal@aldebaran-robotics.com>, Corentin Le Molgat <clemolgat@aldebaran-robotics.com>, Arne B�ckmann <arneboe@tzi.de>");
MODULE_DESCRIPTION("A low-level driver for Aptina MT9M114 sensors");
MODULE_LICENSE("GPL");

#define DRIVER_NAME   "mt9m114"

static int debug = 0;
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

#define dprintk(level, name,  fmt, arg...)\
    do { \
  printk(KERN_DEBUG "%s/0: " fmt, name, ## arg);\
    } while (0)

/*
 * Basic window sizes.  These probably belong somewhere more globally
 * useful.
 */
#define WXGA_WIDTH      1280
#define WXGA_HEIGHT     720
#define FULL_HEIGHT	960
#define VGA_WIDTH	640
#define VGA_HEIGHT      480
#define QVGA_WIDTH      320
#define QVGA_HEIGHT     240
#define CIF_WIDTH       352
#define CIF_HEIGHT      288
#define QCIF_WIDTH      176
#define	QCIF_HEIGHT     144


/*
 * Our nominal (default) frame rate.
 */
#define MT9M114_FRAME_RATE 256

/*
 * The MT9M114 sits on i2c with ID 0x48 or 0x5D
 * depends on input SADDR
 */
#define MT9M114_I2C_ADDR 0x48

/* Registers */

#define REG_CHIP_ID                               0x0000
#define REG_MON_MAJOR_VERSION                     0x8000
#define REG_MON_MINOR_VERION                      0x8002
#define REG_MON_RELEASE_VERSION                   0x8004
#define REG_RESET_AND_MISC_CONTROL                0x001A
#define REG_PAD_SLEW_CONTROL                      0x001E
#define REG_COMMAND_REGISTER                      0x0080
#define   HOST_COMMAND_APPLY_PATCH                0x0001
#define   HOST_COMMAND_SET_STATE                  0x0002
#define   HOST_COMMAND_REFRESH                    0x0004
#define   HOST_COMMAND_WAIT_FOR_EVENT             0x0008
#define   HOST_COMMAND_OK                         0x8000
#define REG_ACCESS_CTL_STAT                       0x0982
#define REG_PHYSICAL_ADDRESS_ACCESS               0x098A
#define REG_LOGICAL_ADDRESS_ACCESS                0x098E
#define MCU_VARIABLE_DATA0                        0x0990
#define MCU_VARIABLE_DATA1                        0x0992
#define REG_RESET_REGISTER                        0x301A  
#define REG_DAC_TXLO_ROW                          0x316A 
#define REG_DAC_TXLO                              0x316C 
#define REG_DAC_LD_4_5                            0x3ED0
#define REG_DAC_LD_6_7                            0x3ED2
#define REG_DAC_ECL                               0x316E
#define REG_DELTA_DK_CONTROL                      0x3180
#define REG_SAMP_COL_PUP2                         0x3E14
#define REG_COLUMN_CORRECTION                     0x30D4
#define REG_LL_ALGO                               0xBC04
#define LL_EXEC_DELTA_DK_CORRECTION               0x0200
#define REG_CAM_DGAIN_RED                         0xC840
#define REG_CAM_DGAIN_GREEN_1                     0xC842
#define REG_CAM_DGAIN_GREEN_2                     0xC844
#define REG_CAM_DGAIN_BLUE                        0xC846

#define REG_CAM_SYSCTL_PLL_ENABLE                 0xC97E 
#define REG_CAM_SYSCTL_PLL_DIVIDER_M_N            0xC980
#define REG_CAM_SYSCTL_PLL_DIVIDER_P              0xC982
#define REG_CAM_SENSOR_CFG_Y_ADDR_START           0xC800
#define REG_CAM_SENSOR_CFG_X_ADDR_START           0xC802
#define REG_CAM_SENSOR_CFG_Y_ADDR_END             0xC804
#define REG_CAM_SENSOR_CFG_X_ADDR_END             0xC806
#define REG_CAM_SENSOR_CFG_PIXCLK                 0xC808 
#define REG_CAM_SENSOR_CFG_ROW_SPEED              0xC80C 
#define REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN    0xC80E 
#define REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX    0xC810 
#define REG_CAM_SENSOR_CFG_FRAME_LENGTH_LINES     0xC812
#define REG_CAM_SENSOR_CFG_LINE_LENGTH_PCK        0xC814
#define REG_CAM_SENSOR_CFG_FINE_CORRECTION        0xC816
#define REG_CAM_SENSOR_CFG_CPIPE_LAST_ROW         0xC818
#define REG_CAM_SENSOR_CFG_REG_0_DATA             0xC826
#define REG_CAM_SENSOR_CONTROL_READ_MODE          0xC834

#define   CAM_SENSOR_CONTROL_VERT_FLIP_EN         0x0002
#define   CAM_SENSOR_CONTROL_HORZ_FLIP_EN         0x0001
#define   CAM_SENSOR_CONTROL_BINNING_EN           0x0330
#define   CAM_SENSOR_CONTROL_SKIPPING_EN          0x0110
#define   CAM_MON_HEARTBEAT                       0x8006 //the frame counter. updates on vertical blanking.

#define REG_CAM_CROP_WINDOW_XOFFSET               0xC854
#define REG_CAM_CROP_WINDOW_YOFFSET               0xC856
#define REG_CAM_CROP_WINDOW_WIDTH                 0xC858
#define REG_CAM_CROP_WINDOW_HEIGHT                0xC85A
#define REG_CAM_CROP_CROPMODE                     0xC85C
#define REG_CAM_OUTPUT_WIDTH                      0xC868
#define REG_CAM_OUTPUT_HEIGHT                     0xC86A
#define REG_CAM_OUTPUT_FORMAT                     0xC86C
#define REG_CAM_OUTPUT_OFFSET                     0xC870
#define REG_CAM_PORT_OUTPUT_CONTROL               0xC984
#define REG_CAM_OUPUT_FORMAT_YUV                  0xC86E
#define REG_CAM_STAT_AWB_CLIP_WINDOW_XSTART       0xC914
#define REG_CAM_STAT_AWB_CLIP_WINDOW_YSTART       0xC916
#define REG_CAM_STAT_AWB_CLIP_WINDOW_XEND         0xC918
#define REG_CAM_STAT_AWB_CLIP_WINDOW_YEND         0xC91A
#define REG_CAM_STAT_AE_INITIAL_WINDOW_XSTART     0xC91C
#define REG_CAM_STAT_AE_INITIAL_WINDOW_YSTART     0xC91E
#define REG_CAM_STAT_AE_INITIAL_WINDOW_XEND       0xC920
#define REG_CAM_STAT_AE_INITIAL_WINDOW_YEND       0xC922
#define REG_CAM_PGA_PGA_CONTROL                   0xC95E
#define REG_SYSMGR_NEXT_STATE                     0xDC00
#define REG_SYSMGR_CURRENT_STATE                  0xDC01
#define REG_PATCHLDR_LOADER_ADDRESS               0xE000
#define REG_PATCHLDR_PATCH_ID                     0xE002
#define REG_PATCHLDR_FIRMWARE_ID                  0xE004
#define REG_PATCHLDR_APPLY_STATUS                 0xE008
#define REG_AUTO_BINNING_MODE                     0xE801
#define REG_CAM_SENSOR_CFG_MAX_ANALOG_GAIN        0xC81C
#define REG_CROP_CROPMODE                         0xC85C 
#define REG_CAM_AET_AEMODE                        0xC878 
#define REG_CAM_AET_TARGET_AVG_LUMA               0xC87A
#define REG_CAM_AET_TARGET_AVERAGE_LUMA_DARK      0xC87B
#define REG_CAM_AET_BLACK_CLIPPING_TARGET         0xC87C
#define REG_CAM_AET_AE_MAX_VIRT_AGAIN             0xC886
#define REG_CAM_AET_MAX_FRAME_RATE                0xC88C
#define REG_CAM_AET_MIN_FRAME_RATE                0xC88E
#define REG_CAM_AET_TARGET_GAIN                   0xC890
#define REG_AE_ALGORITHM                          0xA404
#define REG_AE_TRACK_MODE                         0xA802
#define REG_AE_TRACK_AE_TRACKING_DAMPENING_SPEED  0xA80A

#define REG_CAM_LL_START_BRIGHTNESS               0xC926
#define REG_CAM_LL_STOP_BRIGHTNESS                0xC928
#define REG_CAM_LL_START_GAIN_METRIC              0xC946
#define REG_CAM_LL_STOP_GAIN_METRIC               0xC948
#define REG_CAM_LL_START_TARGET_LUMA_BM           0xC952
#define REG_CAM_LL_STOP_TARGET_LUMA_BM            0xC954
#define REG_CAM_LL_START_SATURATION               0xC92A
#define REG_CAM_LL_END_SATURATION                 0xC92B
#define REG_CAM_LL_START_DESATURATION             0xC92C
#define REG_CAM_LL_END_DESATURATION               0xC92D
#define REG_CAM_LL_START_DEMOSAIC                 0xC92E
#define REG_CAM_LL_START_AP_GAIN                  0xC92F
#define REG_CAM_LL_START_AP_THRESH                0xC930
#define REG_CAM_LL_STOP_DEMOSAIC                  0xC931
#define REG_CAM_LL_STOP_AP_GAIN                   0xC932
#define REG_CAM_LL_STOP_AP_THRESH                 0xC933
#define REG_CAM_LL_START_NR_RED                   0xC934
#define REG_CAM_LL_START_NR_GREEN                 0xC935
#define REG_CAM_LL_START_NR_BLUE                  0xC936
#define REG_CAM_LL_START_NR_THRESH                0xC937
#define REG_CAM_LL_STOP_NR_RED                    0xC938
#define REG_CAM_LL_STOP_NR_GREEN                  0xC939
#define REG_CAM_LL_STOP_NR_BLUE                   0xC93A
#define REG_CAM_LL_STOP_NR_THRESH                 0xC93B
#define REG_CAM_LL_START_CONTRAST_BM              0xC93C
#define REG_CAM_LL_STOP_CONTRAST_BM               0xC93E
#define REG_CAM_LL_GAMMA                          0xC940
#define REG_CAM_LL_START_CONTRAST_GRADIENT        0xC942
#define REG_CAM_LL_STOP_CONTRAST_GRADIENT         0xC943
#define REG_CAM_LL_START_CONTRAST_LUMA_PERCENTAGE 0xC944
#define REG_CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE  0xC945
#define REG_CAM_LL_START_FADE_TO_BLACK_LUMA       0xC94A
#define REG_CAM_LL_STOP_FADE_TO_BLACK_LUMA        0xC94C
#define REG_CAM_LL_CLUSTER_DC_TH_BM               0xC94E
#define REG_CAM_LL_CLUSTER_DC_GATE_PERCENTAGE     0xC950
#define REG_CAM_LL_SUMMING_SENSITIVITY_FACTOR     0xC951
#define REG_CAM_LL_MODE                           0xBC02 //might be BC07.
#define REG_CCM_DELTA_GAIN                        0xB42A



#define REG_CAM_HUE_ANGLE                         0xC873

// AWB
#define REG_AWB_AWB_MODE                          0xC909
#define REG_AWB_COL_TEMP                          0xC8F0//color temp, only writeable if awb mode is manual. in kelvin
#define REG_AWB_COL_TEMP_MAX                      0xC8EE//maximum color temp in kelvin
#define REG_AWB_COL_TEMP_MIN                      0xC8EC//minimum color temp in kelvin

// UVC
#define REG_UVC_AE_MODE                           0xCC00
#define REG_UVC_AUTO_WHITE_BALANCE_TEMPERATURE    0xCC01
#define REG_UVC_AE_PRIORITY                       0xCC02
#define REG_UVC_POWER_LINE_FREQUENCY              0xCC03
#define REG_UVC_EXPOSURE_TIME                     0xCC04
#define REG_UVC_BACKLIGHT_COMPENSATION            0xCC08
#define REG_UVC_BRIGHTNESS                        0xCC0A //set brightness in auto exposure mode.

#define REG_UVC_CONTRAST                          0xCC0C //not exactly what the name suggests. See chip documentation 

#define REG_UVC_GAIN                              0xCC0E
#define REG_UVC_HUE                               0xCC10
#define REG_UVC_SATURATION                        0xCC12
#define REG_UVC_SHARPNESS                         0xCC14 
#define REG_UVC_GAMMA                             0xCC16
#define REG_UVC_WHITE_BALANCE_TEMPERATURE         0xCC18
#define REG_UVC_FRAME_INTERVAL                    0xCC1C
#define REG_UVC_MANUAL_EXPOSURE                   0xCC20
#define REG_UVC_FLICKER_AVOIDANCE                 0xCC21
#define REG_UVC_ALGO                              0xCC22
#define REG_UVC_RESULT_STATUS                     0xCC24

/**This variable selects the system event that the host wishes to wait for.
 * 1: end of frame
 * 2: start of frame */
#define REG_CMD_HANDLER_WAIT_FOR_EVENT            0xFC00

/** This variable determines the number of system event occurrences for which the
 *  Command Handler component will wait */
#define REG_CMD_HANDLER_NUM_WAIT_EVENTS           0xFC02

/**Result status code for last refresh command. Updates after refresh command.
 * Possible values:
   0x00: ENOERR - refresh successful
   0x13: EINVCROPX - invalid horizontal crop configuration
   0x14: EINVCROPY - invalid vertical crop configuration
   0x15: EINVTC - invalid Tilt Correction percentage
*/

#define REG_SEQ_ERROR_CODE                        0x8406


/* SYS_STATE values (for SYSMGR_NEXT_STATE and SYSMGR_CURRENT_STATE) */
#define MT9M114_SYS_STATE_ENTER_CONFIG_CHANGE           0x28
#define MT9M114_SYS_STATE_STREAMING                     0x31
#define MT9M114_SYS_STATE_START_STREAMING               0x34
#define MT9M114_SYS_STATE_ENTER_SUSPEND                 0x40
#define MT9M114_SYS_STATE_SUSPENDED                     0x41
#define MT9M114_SYS_STATE_ENTER_STANDBY                 0x50
#define MT9M114_SYS_STATE_STANDBY                       0x52
#define MT9M114_SYS_STATE_LEAVE_STANDBY                 0x54

//Custom V4L control variables
#define V4L2_MT9M114_FADE_TO_BLACK (V4L2_CID_PRIVATE_BASE) //boolean, enable or disable fade to black feature


/**
 * This enum is used as index for the state's uvc_register_out_of_sync array.
 */
typedef enum {
    UVC_EXPOSURE_TIME,
    UVC_GAIN,
    UVC_BRIGHTNESS,
    UVC_CONTRAST,
    UVC_SATURATION,
    UVC_SHARPNESS,
    NUM_OF_UVC_REGISTERS /** < This value should always be the last! */
} uvc_registers;




/*
 * Information we maintain about a known sensor.
 */
struct mt9m114_format_struct;  /* coming later */
struct mt9m114_info {
  struct v4l2_subdev sd;
  struct mt9m114_format_struct *fmt;  /* Current format */
  unsigned char sat;		/* Saturation value */
  int hue;			/* Hue value */
  int flag_vflip;              /* flip vertical */
  int flag_hflip;              /* flip horizontal */
  
  /* The change config command sometimes breaks the sync between uvc registers and cam
   * variables. This array keeps track of which uvc registers are out of sync 
   * and which are not. Use the uvc_registers enum to access this array */
  bool uvc_register_out_of_sync[NUM_OF_UVC_REGISTERS]; 
};

static inline struct mt9m114_info *to_state(struct v4l2_subdev *sd)
{
  return container_of(sd, struct mt9m114_info, sd);
}



/*
 * The default register settings. There
 * is really no making sense of most of these - lots of "reserved" values
 * and such.
 * FIXME this might be a leftover from the old omnivision driver?!
 * These settings give VGA YUYV.
 */

struct regval_list {
  u16 reg_num;
  u16 size;
  u32 value;
};

static struct regval_list pga_regs[] = {
  { 0x098E, 2, 0},
  { 0xC95E, 2, 3},
  { 0xC95E, 2, 2},
  { 0x3640, 2, 368},
  { 0x3642, 2, 3787},
  { 0x3644, 2, 22480},
  { 0x3646, 2, 33549},
  { 0x3648, 2, 62062},
  { 0x364A, 2, 32303},
  { 0x364C, 2, 18603},
  { 0x364E, 2, 26192},
  { 0x3650, 2, 52556},
  { 0x3652, 2, 44686},
  { 0x3654, 2, 32431},
  { 0x3656, 2, 23244},
  { 0x3658, 2, 7056},
  { 0x365A, 2, 64140},
  { 0x365C, 2, 37614},
  { 0x365E, 2, 32207},
  { 0x3660, 2, 19178},
  { 0x3662, 2, 26800},
  { 0x3664, 2, 45101},
  { 0x3666, 2, 43151},
  { 0x3680, 2, 13964},
  { 0x3682, 2, 1869},
  { 0x3684, 2, 9871},
  { 0x3686, 2, 32394},
  { 0x3688, 2, 38832},
  { 0x368A, 2, 492},
  { 0x368C, 2, 2894},
  { 0x368E, 2, 4687},
  { 0x3690, 2, 45006},
  { 0x3692, 2, 34192},
  { 0x3694, 2, 973},
  { 0x3696, 2, 2349},
  { 0x3698, 2, 25323},
  { 0x369A, 2, 41294},
  { 0x369C, 2, 46959},
  { 0x369E, 2, 3405},
  { 0x36A0, 2, 47531},
  { 0x36A2, 2, 38860},
  { 0x36A4, 2, 22506},
  { 0x36A6, 2, 37359},
  { 0x36C0, 2, 3569},
  { 0x36C2, 2, 36620},
  { 0x36C4, 2, 30224},
  { 0x36C6, 2, 11116},
  { 0x36C8, 2, 42739},
  { 0x36CA, 2, 1681},
  { 0x36CC, 2, 61514},
  { 0x36CE, 2, 13265},
  { 0x36D0, 2, 44462},
  { 0x36D2, 2, 51635},
  { 0x36D4, 2, 23184},
  { 0x36D6, 2, 39789},
  { 0x36D8, 2, 22480},
  { 0x36DA, 2, 3885},
  { 0x36DC, 2, 64882},
  { 0x36DE, 2, 3505},
  { 0x36E0, 2, 46314},
  { 0x36E2, 2, 26864},
  { 0x36E4, 2, 36813},
  { 0x36E6, 2, 41555},
  { 0x3700, 2, 1325},
  { 0x3702, 2, 60557},
  { 0x3704, 2, 46961},
  { 0x3706, 2, 13199},
  { 0x3708, 2, 25234},
  { 0x370A, 2, 10253},
  { 0x370C, 2, 36912},
  { 0x370E, 2, 46449},
  { 0x3710, 2, 17713},
  { 0x3712, 2, 19282},
  { 0x3714, 2, 10509},
  { 0x3716, 2, 53295},
  { 0x3718, 2, 38417},
  { 0x371A, 2, 8881},
  { 0x371C, 2, 26834},
  { 0x371E, 2, 27981},
  { 0x3720, 2, 39469},
  { 0x3722, 2, 34321},
  { 0x3724, 2, 5232},
  { 0x3726, 2, 20978},
  { 0x3740, 2, 35307},
  { 0x3742, 2, 49806},
  { 0x3744, 2, 62036},
  { 0x3746, 2, 23250},
  { 0x3748, 2, 27830},
  { 0x374A, 2, 8111},
  { 0x374C, 2, 51085},
  { 0x374E, 2, 33653},
  { 0x3750, 2, 24914},
  { 0x3752, 2, 29270},
  { 0x3754, 2, 5133},
  { 0x3756, 2, 5933},
  { 0x3758, 2, 52436},
  { 0x375A, 2, 13362},
  { 0x375C, 2, 18166},
  { 0x375E, 2, 37550},
  { 0x3760, 2, 39566},
  { 0x3762, 2, 61300},
  { 0x3764, 2, 23602},
  { 0x3766, 2, 26198},
  { 0x3782, 2, 480},
  { 0x3784, 2, 672},
  { 0xC960, 2, 2800},
  { 0xC962, 2, 31149},
  { 0xC964, 2, 22448},
  { 0xC966, 2, 30936},
  { 0xC968, 2, 29792},
  { 0xC96A, 2, 4000},
  { 0xC96C, 2, 33143},
  { 0xC96E, 2, 33116},
  { 0xC970, 2, 33041},
  { 0xC972, 2, 32855},
  { 0xC974, 2, 6500},
  { 0xC976, 2, 31786},
  { 0xC978, 2, 26268},
  { 0xC97A, 2, 32319},
  { 0xC97C, 2, 29650},
  { 0xC95E, 2, 3},
  { 0xffff, 0xffff ,0xffff}
};

static struct regval_list ccm_awb_regs[] = {
  { 0xC892, 2, 615},
  { 0xC894, 2, 65306},
  { 0xC896, 2, 65459},
  { 0xC898, 2, 65408},
  { 0xC89A, 2, 358},
  { 0xC89C, 2, 3},
  { 0xC89E, 2, 65434},
  { 0xC8A0, 2, 65204},
  { 0xC8A2, 2, 589},
  { 0xC8A4, 2, 447},
  { 0xC8A6, 2, 65281},
  { 0xC8A8, 2, 65523},
  { 0xC8AA, 2, 65397},
  { 0xC8AC, 2, 408},
  { 0xC8AE, 2, 65533},
  { 0xC8B0, 2, 65434},
  { 0xC8B2, 2, 65255},
  { 0xC8B4, 2, 680},
  { 0xC8B6, 2, 473},
  { 0xC8B8, 2, 65318},
  { 0xC8BA, 2, 65523},
  { 0xC8BC, 2, 65459},
  { 0xC8BE, 2, 306},
  { 0xC8C0, 2, 65512},
  { 0xC8C2, 2, 65498},
  { 0xC8C4, 2, 65229},
  { 0xC8C6, 2, 706},
  { 0xC8C8, 2, 117},
  { 0xC8CA, 2, 284},
  { 0xC8CC, 2, 154},
  { 0xC8CE, 2, 261},
  { 0xC8D0, 2, 164},
  { 0xC8D2, 2, 172},
  { 0xC8D4, 2, 2700},
  { 0xC8D6, 2, 3850},
  { 0xC8D8, 2, 6500},
  { 0xC914, 2, 0},
  { 0xC916, 2, 0},
  { 0xC918, 2, 1279},
  { 0xC91A, 2, 719},
  { 0xC904, 2, 51},
  { 0xC906, 2, 64},
  { 0xC8F2, 1, 3},
  { 0xC8F3, 1, 2},
  { 0xC906, 2, 60},
  { 0xC8F4, 2, 0},
  { 0xC8F6, 2, 0},
  { 0xC8F8, 2, 0},
  { 0xC8FA, 2, 59172},
  { 0xC8FC, 2, 5507},
  { 0xC8FE, 2, 8261},
  { 0xC900, 2, 1023},
  { 0xC902, 2, 124},
  { 0xC90C, 1, 128},
  { 0xC90D, 1, 128},
  { 0xC90E, 1, 128},
  { 0xC90F, 1, 136},
  { 0xC910, 1, 128},
  { 0xC911, 1, 128},
  { 0xffff, 0xffff ,0xffff}
};


static struct regval_list uvc_ctrl_regs[] = {
  { REG_UVC_AE_MODE, 1, 0x02}, //has to be enabled by default, otherwise the camera will never start
  { REG_UVC_AUTO_WHITE_BALANCE_TEMPERATURE, 1, 0x01},
  { REG_UVC_AE_PRIORITY, 1, 0x00},
  { REG_UVC_POWER_LINE_FREQUENCY, 1, 0x02},
  { REG_UVC_EXPOSURE_TIME, 4, 0x00000001},
  { REG_UVC_BACKLIGHT_COMPENSATION, 2, 0x0001},
  { REG_UVC_BRIGHTNESS, 2, 0x0037},
  { REG_UVC_CONTRAST, 2, 0x0020},
  { REG_UVC_GAIN, 2, 0x0020},
  { REG_UVC_HUE, 2, 0x0000},
  { REG_UVC_SATURATION, 2, 0x0080},
  { REG_UVC_SHARPNESS, 2, -7},
  { REG_UVC_GAMMA, 2, 0x00DC},
  { REG_UVC_WHITE_BALANCE_TEMPERATURE, 2, 0x09C4},
  { REG_UVC_FRAME_INTERVAL, 4, 0x00000001},
  { REG_UVC_MANUAL_EXPOSURE, 1, 0x00}, //disable flicker avoidance, allow exposure time to be longer than the frame time.
  { REG_UVC_FLICKER_AVOIDANCE, 1, 0x00},
  { REG_UVC_ALGO, 2, 0x0007},
  { REG_UVC_RESULT_STATUS, 1, 0x00},
  { 0xffff, 0xffff ,0xffff}
};



static struct regval_list mt9m114_960p30_regs[] = {
  {REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000},
  {REG_CAM_SENSOR_CFG_Y_ADDR_START, 2, 4},
  {REG_CAM_SENSOR_CFG_X_ADDR_START, 2, 4},
  {REG_CAM_SENSOR_CFG_Y_ADDR_END, 2, 971},
  {REG_CAM_SENSOR_CFG_X_ADDR_END, 2, 1291},
  {REG_CAM_SENSOR_CFG_PIXCLK, 4, 48000000},
  {REG_CAM_SENSOR_CFG_ROW_SPEED, 2, 0x0001},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 2, 219},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 2, 1480},
  {REG_CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 2, 1007},
  {REG_CAM_SENSOR_CFG_LINE_LENGTH_PCK, 2, 1611},
  {REG_CAM_SENSOR_CFG_FINE_CORRECTION, 2, 96},
  {REG_CAM_SENSOR_CFG_CPIPE_LAST_ROW, 2, 963},
  {REG_CAM_SENSOR_CFG_REG_0_DATA, 2, 0x0020},
 // {REG_CAM_SENSOR_CONTROL_READ_MODE, 1, 0x0000},
  {REG_CAM_CROP_WINDOW_XOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_YOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_WIDTH, 2, 1280},
  {REG_CAM_CROP_WINDOW_HEIGHT, 2, 960},
  {REG_CROP_CROPMODE, 1, 0x03},
  {REG_CAM_OUTPUT_WIDTH, 2, 1280},
  {REG_CAM_OUTPUT_HEIGHT, 2, 960},
  {REG_CAM_AET_AEMODE, 1, 0x0},
  {REG_CAM_AET_MAX_FRAME_RATE, 2, 0x1D97},
  {REG_CAM_AET_MIN_FRAME_RATE, 2, 0x1D97},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XEND, 2, 1279},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YEND, 2, 959},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XEND, 2, 255},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YEND, 2, 191},
  { 0xffff, 0xffff ,0xffff }	/* END MARKER */
};


static struct regval_list mt9m114_720p36_regs[] = {
  {REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000},
  {REG_CAM_SENSOR_CFG_Y_ADDR_START, 2, 124},
  {REG_CAM_SENSOR_CFG_X_ADDR_START, 2, 4},
  {REG_CAM_SENSOR_CFG_Y_ADDR_END, 2, 851},
  {REG_CAM_SENSOR_CFG_X_ADDR_END, 2, 1291},
  {REG_CAM_SENSOR_CFG_PIXCLK, 4, 48000000},
  {REG_CAM_SENSOR_CFG_ROW_SPEED, 2, 0x0001},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 2, 219},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 2, 1558},
  {REG_CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 2, 778},
  {REG_CAM_SENSOR_CFG_LINE_LENGTH_PCK, 2, 1689},
  {REG_CAM_SENSOR_CFG_FINE_CORRECTION, 2, 96},
  {REG_CAM_SENSOR_CFG_CPIPE_LAST_ROW, 2, 723},
  {REG_CAM_SENSOR_CFG_REG_0_DATA, 2, 0x0020},
 // {REG_CAM_SENSOR_CONTROL_READ_MODE, 1, 0x0000},
  {REG_CAM_CROP_WINDOW_XOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_YOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_WIDTH, 2, 1280},
  {REG_CAM_CROP_WINDOW_HEIGHT, 2, 720},
  {REG_CROP_CROPMODE, 1, 0x03},
  {REG_CAM_OUTPUT_WIDTH, 2, 1280},
  {REG_CAM_OUTPUT_HEIGHT, 2, 720},
  {REG_CAM_AET_AEMODE, 1, 0x00},
  {REG_CAM_AET_MAX_FRAME_RATE, 2, 0x24AB},
  {REG_CAM_AET_MIN_FRAME_RATE, 2, 0x24AB},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XEND, 2, 1279},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YEND, 2, 719},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XEND, 2, 255},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YEND, 2, 143},
  { 0xffff, 0xffff ,0xffff }	/* END MARKER */
};

#if 0
static struct regval_list mt9m114_vga_30_to_75_binned_regs[] = {
  {REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000},
  {REG_CAM_SENSOR_CFG_Y_ADDR_START, 2, 0},
  {REG_CAM_SENSOR_CFG_X_ADDR_START, 2, 0},
  {REG_CAM_SENSOR_CFG_Y_ADDR_END, 2, 973},
  {REG_CAM_SENSOR_CFG_X_ADDR_END, 2, 1293},
  {REG_CAM_SENSOR_CFG_PIXCLK, 4, 48000000},
  {REG_CAM_SENSOR_CFG_ROW_SPEED, 2, 1},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 2, 451},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 2, 948},
  {REG_CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 2, 541},
  {REG_CAM_SENSOR_CFG_LINE_LENGTH_PCK, 2, 1183},
  {REG_CAM_SENSOR_CFG_FINE_CORRECTION, 2, 224},
  {REG_CAM_SENSOR_CFG_CPIPE_LAST_ROW, 2, 484},
  {REG_CAM_SENSOR_CFG_REG_0_DATA, 2, 0x0020},
  //{REG_CAM_SENSOR_CONTROL_READ_MODE, 1, 0x0000},
  {REG_CAM_CROP_WINDOW_XOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_YOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_WIDTH, 2, 640},
  {REG_CAM_CROP_WINDOW_HEIGHT, 2, 481},
  {REG_CROP_CROPMODE, 1, 3},
  {REG_CAM_OUTPUT_WIDTH, 2, 640},
  {REG_CAM_OUTPUT_HEIGHT, 2, 481},
  {REG_CAM_AET_AEMODE, 1, 0x00},
  {REG_CAM_AET_MAX_FRAME_RATE, 2, 0x4B00},
  {REG_CAM_AET_MIN_FRAME_RATE, 2, 0x1E00},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XEND, 2, 639},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YEND, 2, 480},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XEND, 2, 127},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YEND, 2, 95},
 // {REG_AUTO_BINNING_MODE,1, 0x00},
  { 0xffff, 0xffff ,0xffff }
};
#endif

static struct regval_list mt9m114_vga_30_scaling_regs[] = {
  {REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000},
  {REG_CAM_SENSOR_CFG_Y_ADDR_START, 2, 4},
  {REG_CAM_SENSOR_CFG_X_ADDR_START, 2, 4},
  {REG_CAM_SENSOR_CFG_Y_ADDR_END, 2, 971},
  {REG_CAM_SENSOR_CFG_X_ADDR_END, 2, 1291},
  {REG_CAM_SENSOR_CFG_PIXCLK, 4, 48000000},
  {REG_CAM_SENSOR_CFG_ROW_SPEED, 2, 1},//FIXME according to the documentation this value is unused, however we still set the default. No idea why
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 2, 219},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 2, 1460},
  {REG_CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 2, 1006},//FIXME might be a typo. default value is 1007
  {REG_CAM_SENSOR_CFG_LINE_LENGTH_PCK, 2, 1591},//FIXME might be a typo? default is 1589
  {REG_CAM_SENSOR_CFG_FINE_CORRECTION, 2, 96},
  {REG_CAM_SENSOR_CFG_CPIPE_LAST_ROW, 2, 963}, 
  {REG_CAM_SENSOR_CFG_REG_0_DATA, 2, 0x0020},
 // {REG_CAM_SENSOR_CONTROL_READ_MODE, 1, 0x0000},
  {REG_CAM_CROP_WINDOW_XOFFSET, 2, 0x0000}, 
  {REG_CAM_CROP_WINDOW_YOFFSET, 2, 0x0000}, 
  {REG_CAM_CROP_WINDOW_WIDTH, 2, 1280},
  {REG_CAM_CROP_WINDOW_HEIGHT, 2, 960},
  {REG_CROP_CROPMODE, 1, 3},
  {REG_CAM_OUTPUT_WIDTH, 2, 640},
  {REG_CAM_OUTPUT_HEIGHT, 2, 480},
  {REG_CAM_AET_AEMODE, 1, 0x00},
  {REG_CAM_AET_MAX_FRAME_RATE, 2, 0x1DFD},
  {REG_CAM_AET_MIN_FRAME_RATE, 2, 0x1DFD},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XEND, 2, 639},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YEND, 2, 479},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XEND, 2, 127},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YEND, 2, 95},
 // {REG_AUTO_BINNING_MODE,1, 0x00},
  { 0xffff, 0xffff ,0xffff }//array end, 
};

#if 0
static struct regval_list mt9m114_qvga_30_to_120_binned_regs[] = {
  {REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000},
  {REG_CAM_SENSOR_CFG_Y_ADDR_START, 2, 238},
  {REG_CAM_SENSOR_CFG_X_ADDR_START, 2, 320},
  {REG_CAM_SENSOR_CFG_Y_ADDR_END, 2, 733},
  {REG_CAM_SENSOR_CFG_X_ADDR_END, 2, 973},
  {REG_CAM_SENSOR_CFG_PIXCLK, 4, 48000000},
  {REG_CAM_SENSOR_CFG_ROW_SPEED, 2, 1},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 2, 451},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 2, 648},
  {REG_CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 2, 453},
  {REG_CAM_SENSOR_CFG_LINE_LENGTH_PCK, 2, 883},
  {REG_CAM_SENSOR_CFG_FINE_CORRECTION, 2, 224},
  {REG_CAM_SENSOR_CFG_CPIPE_LAST_ROW, 2, 244},
  {REG_CAM_SENSOR_CFG_REG_0_DATA, 2, 0x0020},
 // {REG_CAM_SENSOR_CONTROL_READ_MODE, 1, 0x0000},
  {REG_CAM_CROP_WINDOW_XOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_YOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_WIDTH, 2, 320},
  {REG_CAM_CROP_WINDOW_HEIGHT, 2, 241},
  {REG_CROP_CROPMODE, 1, 3},
  {REG_CAM_OUTPUT_WIDTH, 2, 320},
  {REG_CAM_OUTPUT_HEIGHT, 2, 241},
  {REG_CAM_AET_AEMODE, 1, 0x00},
  {REG_CAM_AET_MAX_FRAME_RATE, 2, 0x7800},
  {REG_CAM_AET_MIN_FRAME_RATE, 2, 0x1E00},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XEND, 2, 319},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YEND, 2, 240},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XEND, 2, 63},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YEND, 2, 47},
//  {REG_AUTO_BINNING_MODE,1, 0x00},

  { 0xffff, 0xffff ,0xffff }
};
#endif

static struct regval_list mt9m114_qvga_30_scaling_regs[] = {
  {REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000},
  {REG_CAM_SENSOR_CFG_Y_ADDR_START, 2, 4},
  {REG_CAM_SENSOR_CFG_X_ADDR_START, 2, 4},
  {REG_CAM_SENSOR_CFG_Y_ADDR_END, 2, 971},
  {REG_CAM_SENSOR_CFG_X_ADDR_END, 2, 1291},
  {REG_CAM_SENSOR_CFG_PIXCLK, 4, 48000000},
  {REG_CAM_SENSOR_CFG_ROW_SPEED, 2, 1},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 2, 219},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 2, 1460},
  {REG_CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 2, 1006},
  {REG_CAM_SENSOR_CFG_LINE_LENGTH_PCK, 2, 1591},
  {REG_CAM_SENSOR_CFG_FINE_CORRECTION, 2, 96},
  {REG_CAM_SENSOR_CFG_CPIPE_LAST_ROW, 2, 963},
  {REG_CAM_SENSOR_CFG_REG_0_DATA, 2, 0x0020},
  //{REG_CAM_SENSOR_CONTROL_READ_MODE, 1, 0x0000},
  {REG_CAM_CROP_WINDOW_XOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_YOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_WIDTH, 2, 1280},
  {REG_CAM_CROP_WINDOW_HEIGHT, 2, 960},
  {REG_CROP_CROPMODE, 1, 3},
  {REG_CAM_OUTPUT_WIDTH, 2, 320},
  {REG_CAM_OUTPUT_HEIGHT, 2, 240},
  {REG_CAM_AET_AEMODE, 1, 0x00},
  {REG_CAM_AET_MAX_FRAME_RATE, 2, 0x1DFD},
  {REG_CAM_AET_MIN_FRAME_RATE, 2, 0x1DFD},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XEND, 2, 319},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YEND, 2, 239},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XEND, 2, 63},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YEND, 2, 47},
//  {REG_AUTO_BINNING_MODE,1, 0x00},

  { 0xffff, 0xffff ,0xffff }
};

#if 0
static struct regval_list mt9m114_160x120_30_to_120_binned_regs[] = {
  {REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000},
  {REG_CAM_SENSOR_CFG_Y_ADDR_START, 2, 358},
  {REG_CAM_SENSOR_CFG_X_ADDR_START, 2, 480},
  {REG_CAM_SENSOR_CFG_Y_ADDR_END, 2, 613},
  {REG_CAM_SENSOR_CFG_X_ADDR_END, 2, 813},
  {REG_CAM_SENSOR_CFG_PIXCLK, 4, 48000000},
  {REG_CAM_SENSOR_CFG_ROW_SPEED, 2, 1},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 2, 451},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 2, 648},
  {REG_CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 2, 453},
  {REG_CAM_SENSOR_CFG_LINE_LENGTH_PCK, 2, 883},
  {REG_CAM_SENSOR_CFG_FINE_CORRECTION, 2, 224},
  {REG_CAM_SENSOR_CFG_CPIPE_LAST_ROW, 2, 124},
  {REG_CAM_SENSOR_CFG_REG_0_DATA, 2, 0x0020},
  //{REG_CAM_SENSOR_CONTROL_READ_MODE, 1, 0x0000},
  {REG_CAM_CROP_WINDOW_XOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_YOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_WIDTH, 2, 160},
  {REG_CAM_CROP_WINDOW_HEIGHT, 2, 121},
  {REG_CROP_CROPMODE, 1, 3},
  {REG_CAM_OUTPUT_WIDTH, 2, 160},
  {REG_CAM_OUTPUT_HEIGHT, 2, 121},
  {REG_CAM_AET_AEMODE, 1, 0x00},
  {REG_CAM_AET_MAX_FRAME_RATE, 2, 0x7800},
  {REG_CAM_AET_MIN_FRAME_RATE, 2, 0x1E00},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XEND, 2, 159},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YEND, 2, 120},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XEND, 2, 31},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YEND, 2, 23},
//  {REG_AUTO_BINNING_MODE,1, 0x00},

  { 0xffff, 0xffff ,0xffff }
};
#endif

static struct regval_list mt9m114_160x120_30_scaling_regs[] = {
  {REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000},
  {REG_CAM_SENSOR_CFG_Y_ADDR_START, 2, 4},
  {REG_CAM_SENSOR_CFG_X_ADDR_START, 2, 4},
  {REG_CAM_SENSOR_CFG_Y_ADDR_END, 2, 971},
  {REG_CAM_SENSOR_CFG_X_ADDR_END, 2, 1291},
  {REG_CAM_SENSOR_CFG_PIXCLK, 4, 48000000},
  {REG_CAM_SENSOR_CFG_ROW_SPEED, 2, 1},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 2, 219},
  {REG_CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 2, 1460},
  {REG_CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 2, 1006},
  {REG_CAM_SENSOR_CFG_LINE_LENGTH_PCK, 2, 1591},
  {REG_CAM_SENSOR_CFG_FINE_CORRECTION, 2, 96},
  {REG_CAM_SENSOR_CFG_CPIPE_LAST_ROW, 2, 963},
  {REG_CAM_SENSOR_CFG_REG_0_DATA, 2, 0x0020},
  //{REG_CAM_SENSOR_CONTROL_READ_MODE, 1, 0x0000},
  {REG_CAM_CROP_WINDOW_XOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_YOFFSET, 2, 0x0000},
  {REG_CAM_CROP_WINDOW_WIDTH, 2, 1280},
  {REG_CAM_CROP_WINDOW_HEIGHT, 2, 960},
  {REG_CROP_CROPMODE, 1, 3},
  {REG_CAM_OUTPUT_WIDTH, 2, 160},
  {REG_CAM_OUTPUT_HEIGHT, 2, 120},
  {REG_CAM_AET_AEMODE, 1, 0x00},
  {REG_CAM_AET_MAX_FRAME_RATE, 2, 0x1DFD},
  {REG_CAM_AET_MIN_FRAME_RATE, 2, 0x1DFD},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_XEND, 2, 159},
  {REG_CAM_STAT_AWB_CLIP_WINDOW_YEND, 2, 119},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YSTART, 2, 0x0000},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_XEND, 2, 31},
  {REG_CAM_STAT_AE_INITIAL_WINDOW_YEND, 2, 23},
//  {REG_AUTO_BINNING_MODE,1, 0x00},

  { 0xffff, 0xffff ,0xffff }
};

/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */


static struct regval_list mt9m114_fmt_yuv422[] = {
  {REG_CAM_OUTPUT_FORMAT, 2, 0x000A},
  {REG_CAM_OUTPUT_OFFSET, 1, 0x10},
  {REG_CAM_OUPUT_FORMAT_YUV, 2, 0x1A},
  { 0xffff, 0xffff, 0xffff },
};


//forward declarations
static int mt9m114_wait_num_frames(struct v4l2_subdev *sd, u16 numFrames);
static int mt9m114_g_exposure(struct v4l2_subdev *sd, s32 *value);
static int mt9m114_s_exposure(struct v4l2_subdev *sd, u32 value);
static int mt9m114_g_gain(struct v4l2_subdev *sd, s32 *value);
static int mt9m114_s_gain(struct v4l2_subdev *sd, int value);
static int mt9m114_g_brightness(struct v4l2_subdev *sd, s32 *value);
static int mt9m114_s_brightness(struct v4l2_subdev *sd, int value);
static int mt9m114_g_contrast(struct v4l2_subdev *sd, s32 *value);
static int mt9m114_s_contrast(struct v4l2_subdev *sd, int value);
static int mt9m114_g_sat(struct v4l2_subdev *sd, s32 *value);
static int mt9m114_s_sat(struct v4l2_subdev *sd, int value);
static int mt9m114_g_sharpness(struct v4l2_subdev *sd, s32 *value);
static int mt9m114_s_sharpness(struct v4l2_subdev *sd, int value);
static int mt9m114_g_hue(struct v4l2_subdev *sd, s32 *value);



/*
 * Low-level register I/O.
 */

static int mt9m114_read(struct v4l2_subdev *sd,
                        u16 reg,
                        u16 size,
                        u32 *value)
{
  u8 cmd[10];

  struct i2c_client *client = v4l2_get_subdevdata(sd);
  cmd[0] = reg/256;
  cmd[1] = reg%256;
  i2c_master_send(client, cmd, 2); //FIXME these functions return error codes. Check them.
  i2c_master_recv(client, cmd, size);
  if( size == 2 )
  {
    *value = (((u32)cmd[0])<<8) + cmd[1];
  }
  else if( size == 4 )
  {
    *value = (((u32)cmd[0])<<24) + (((u32)cmd[1])<<16) +
             (((u32)cmd[2])<<8)  + (((u32)cmd[3])<<0);
  }
  else if( size == 1 )
  {
    *value = cmd[0];
  }

  return 0;
}


#define MAX_MASTER_WRITE 48

static int mt9m114_burst_write(struct v4l2_subdev *sd,
                               u16 reg,
                               u16 * array,
                               u16 size)
{
  int i=0;
  int index=0, abs_index=0;
  int packet_size=0;
  u8 cmd[255];

  struct i2c_client *client = v4l2_get_subdevdata(sd);

  while(size)
  {
    if (size >= MAX_MASTER_WRITE){
      packet_size = MAX_MASTER_WRITE;
    }
    else{
      packet_size = size;
    }
    size -= packet_size;
    index = 0;
    cmd[index++] = reg/256;
    cmd[index++] = reg%256;
    for (i = 0;i < packet_size; i++)
    {
      u16 val = array[abs_index++];
      cmd[index++] = val / 256;
      cmd[index++] = val % 256;
      reg +=2;
    }
    i2c_master_send(client, cmd, index);
  }
  return 0;
}

/**
 * 
 * @param sd
 * @param reg
 * @param size
 * @param value
 * @return 0 in case of success. Errno error code otherwise.
 */
static int mt9m114_write(struct v4l2_subdev *sd,
                         u16 reg,
                         u16 size,
                         u32 value)
{

   u8 cmd[10];
   int index=0;
   struct i2c_client *client = v4l2_get_subdevdata(sd);
   int numBytesWritten = 0;
   
   cmd[index++] = reg/256;
   cmd[index++] = reg%256;

   if( size == 2) 
   {
     //FIXME this breaks signedness.
     cmd[index++] = value/256;
     cmd[index++] = value%256;
   }
   else if( size == 4)
   {
     cmd[index++] = value>>24 & 0xff;
     cmd[index++] = value>>16 & 0xff;
     cmd[index++] = value>>8   & 0xff;
     cmd[index++] = value>>0   & 0xff;
   }
   else if( size == 1)
   {
     cmd[index++] = value;
   }
   
   numBytesWritten = i2c_master_send(client, cmd, index); //returns negative errno or the number of bytes written

   if(numBytesWritten != index) //make sure that everything was written
   {
     if(numBytesWritten < 0) //error code
     {
       numBytesWritten *= -1; //in case of error i2c_master_send returns the negative errno.
       dprintk(0,"MT9M114","MT9M114 : i2c send failed. Error code: 0x%x\n", numBytesWritten);
       return numBytesWritten;
     }
     else
     {
       dprintk(0,"MT9M114","MT9M114 : i2c send failed. Wrote %i bytes but should have written %i bytes\n", numBytesWritten, index);
       return EIO;
     }
   }
   else
   {
     return 0;
   }
}


/*
 * Write a list of register settings; ff/ff stops the process.
 */
static int mt9m114_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
  int i=0;

  while ((vals[i].reg_num != 0xffff) || (vals[i].value != 0xffff)) {
    int ret = mt9m114_write(sd, vals[i].reg_num, vals[i].size, vals[i].value);
    if (ret < 0)
    {
      return ret;
    }
    i++;
  }
  return 0;
}

static int mt9m114_errata_1(struct v4l2_subdev *sd)
{
  mt9m114_write(sd, REG_SAMP_COL_PUP2, 2, 0xFF39); //no idea, register is undocumented
  return 0;
}


#define RESET_REGISTER_MASK_BAD 0x0200
static int mt9m114_errata_2(struct v4l2_subdev *sd)
{
  //FIXME this doesn't make sense
  //bit 2 is reserved and the default value is 0 but it is set to 1.
  //bit 5 is reserved, default is 0 but it is set to 1
  //MSB is the actual reset register. No idea why the others are changed.
  mt9m114_write(sd, REG_RESET_REGISTER, 2, 564); //1000110100
  return 0;
}

/**
 * 
 * @param sd
 * @param bit_mask
 * @return 0 if everything is ok, 1 otherwise
 */
static int poll_command_register_bit(struct v4l2_subdev *sd, u16 bit_mask)
{
  int i=0;
  u32 v=0;

  for (i = 0; i < 1000; i++)
  {
    msleep(10);
    mt9m114_read(sd, REG_COMMAND_REGISTER, 2, &v);
    if (!(v & bit_mask))
    {
      return 0;
    }
  }
  return 1;
}

/**
 * Reads the uvc_result_status register. 
 * If it contains an error the error is printed to dmesg together with funcName.
 * 
 * @note uvc_result_status updates on vertical blanking. Therefore it might not
 *       contain the correct value if you do not wait until vertical blanking.
 * @param sd
 * @param funcName Name of the calling function.
 * @return In case of error: The errno code or -1 if the error code is unknown. 0 Otherwise.
 */
static int check_uvc_status(struct v4l2_subdev *sd, const char* funcName)
{
  int result;
  mt9m114_read(sd, REG_UVC_RESULT_STATUS, 1, &result);
  
  
  /* uvc_result_status can contain the following error codes:
     0x00 ENOERR no error - change was accepted and actioned.
     0x08 EACCES permission denied.
     0x09 EBUSY entity busy, cannot support operation.
     0x0C EINVAL invalid argument.
     0x0E ERANGE parameter out-of-range.
     0x0F ENOSYS operation not supported.
   */
  switch(result)
  {
    case 0: //no error
      dprintk(0,"MT9M114","%s REG_UVC_RESULT_STATUS: ENOERR\n", funcName);
      return 0;
    case 0x08: //EACCES
      dprintk(0,"MT9M114","%s REG_UVC_RESULT_STATUS: EACCES\n", funcName);
      return EACCES;
    case 0x09: //EBUSY
      dprintk(0,"MT9M114","%s REG_UVC_RESULT_STATUS: EBUSY\n", funcName);
      return EBUSY;
    case 0x0C: //EINVAL
      dprintk(0,"MT9M114","%s REG_UVC_RESULT_STATUS: EINVAL\n", funcName);
      return EINVAL;
    case 0x0E: //ERANGE
      dprintk(0,"MT9M114","%s REG_UVC_RESULT_STATUS: ERANGE\n", funcName);
      return ERANGE;
    case 0x0F: //ENOSYS
      dprintk(0,"MT9M114","%s REG_UVC_RESULT_STATUS: ENOSYS\n", funcName);
      return ENOSYS;
    default:
      dprintk(0,"MT9M114","%s REG_UVC_RESULT_STATUS: Unknown error code\n", funcName);
      return -1;
  }
}


// Patch 0202; Feature Recommended; Black level correction fix
static int mt9m114_patch2_black_lvl_correction_fix(struct v4l2_subdev *sd)
{
  u32 v=0;

  u16 reg_burst[] = {
    0x70cf, 0xffff, 0xc5d4, 0x903a, 0x2144, 0x0c00, 0x2186, 0x0ff3,
    0xb844, 0xb948, 0xe082, 0x20cc, 0x80e2, 0x21cc, 0x80a2, 0x21cc,
    0x80e2, 0xf404, 0xd801, 0xf003, 0xd800, 0x7ee0, 0xc0f1, 0x08ba,
    0x0600, 0xc1a1, 0x76cf, 0xffff, 0xc130, 0x6e04, 0xc040, 0x71cf,
    0xffff, 0xc790, 0x8103, 0x77cf, 0xffff, 0xc7c0, 0xe001, 0xa103,
    0xd800, 0x0c6a, 0x04e0, 0xb89e, 0x7508, 0x8e1c, 0x0809, 0x0191,
    0xd801, 0xae1d, 0xe580, 0x20ca, 0x0022, 0x20cf, 0x0522, 0x0c5c,
    0x04e2, 0x21ca, 0x0062, 0xe580, 0xd901, 0x79c0, 0xd800, 0x0be6,
    0x04e0, 0xb89e, 0x70cf, 0xffff, 0xc8d4, 0x9002, 0x0857, 0x025e,
    0xffdc, 0xe080, 0x25cc, 0x9022, 0xf225, 0x1700, 0x108a, 0x73cf,
    0xff00, 0x3174, 0x9307, 0x2a04, 0x103e, 0x9328, 0x2942, 0x7140,
    0x2a04, 0x107e, 0x9349, 0x2942, 0x7141, 0x2a04, 0x10be, 0x934a,
    0x2942, 0x714b, 0x2a04, 0x10be, 0x130c, 0x010a, 0x2942, 0x7142,
    0x2250, 0x13ca, 0x1b0c, 0x0284, 0xb307, 0xb328, 0x1b12, 0x02c4,
    0xb34a, 0xed88, 0x71cf, 0xff00, 0x3174, 0x9106, 0xb88f, 0xb106,
    0x210a, 0x8340, 0xc000, 0x21ca, 0x0062, 0x20f0, 0x0040, 0x0b02,
    0x0320, 0xd901, 0x07f1, 0x05e0, 0xc0a1, 0x78e0, 0xc0f1, 0x71cf,
    0xffff, 0xc7c0, 0xd840, 0xa900, 0x71cf, 0xffff, 0xd02c, 0xd81e,
    0x0a5a, 0x04e0, 0xda00, 0xd800, 0xc0d1, 0x7ee0
  };

  mt9m114_write(sd, REG_ACCESS_CTL_STAT, 2, 0x0001);
  mt9m114_write(sd, REG_PHYSICAL_ADDRESS_ACCESS, 2, 0x5000);
  mt9m114_burst_write(sd,0xd000, reg_burst, ARRAY_SIZE(reg_burst));
  mt9m114_write(sd, REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000);
  mt9m114_write(sd, REG_PATCHLDR_LOADER_ADDRESS, 2, 0x010c);
  mt9m114_write(sd, REG_PATCHLDR_PATCH_ID, 2, 0x0202);
  mt9m114_write(sd, REG_PATCHLDR_FIRMWARE_ID, 4, 0x41030202);

  mt9m114_write(sd, REG_COMMAND_REGISTER, 2, HOST_COMMAND_OK);
  v = HOST_COMMAND_OK | HOST_COMMAND_APPLY_PATCH;
  mt9m114_write(sd, REG_COMMAND_REGISTER, 2, v);

  if(poll_command_register_bit(sd,HOST_COMMAND_APPLY_PATCH))
  {
    dprintk(0,"MT9M114","poll apply patch timeout\n");
  }

  mt9m114_read(sd, REG_COMMAND_REGISTER, 2, &v);
  if ( !(v & HOST_COMMAND_OK))
  {
    dprintk(0,"MT9M114","Warning : apply patch 2 Black level correction fix Host_command not OK\n");
  }

  mt9m114_read(sd, REG_PATCHLDR_APPLY_STATUS, 1, &v);
  if (v)
  {
    dprintk(0,"MT9M114","MT9M114 : patch apply 2 Black level correction fix status non-zero  - value:%x\n",v);
    return -1;
  }

  return 0;
}



//
// Patch 03 - Feature request, Adaptive Sensitivity.
//
// This patch implements the new feature VGA auto binning mode. This was a
// request to support automatic mode transition between VGA scaled and binning
// mode (and back)
//
// To support this feature a new Firmware variable page has been added which
// controls this functionality as well as hold configuration parameters for
// the automatic binning mode of operation. This pasge needs to be configured
// correctly as these values will be used to populate the CAM page during the
// switch
//
//
// Main control variables
//     AUTO_BINNING_MODE.AUTO_BINNING_MODE_ENABLE:
//         Controls automatic binning mode (0=disabled, 1=enabled).
//         NOTE: Requires Change-Congig to apply
//     AUTO_BINNING_STATUS.AUTO_BINNING_STATUS_ENABLE
//         Current enable/disable state of automatic binning mode (0=disabled, 1=enabled)
//     AUTO_BINNING_THRESHOLD_BM
//         Switching threshold in terms of inverse brightness metric (ufixed8)
//     AUTO_BINNING_GATE_PERCENTAGE
//         Gate width as a percentage of threshold
//
// Notes:
//     CAM_LL_SUMMING_SENSITIVITY_FACTOR
//         This is the sensitivity gain that is achieved when sub-sampled
//         read mode is selected, summing or average (approximately 2.0x
//         unity=32)
//
//     The sensitivity factor and gate width must be tuned correctly to avoid
//     oscillation during the switch

static int mt9m114_patch3_adaptive_sensitivity(struct v4l2_subdev *sd)
{
  u32 v=0;
  u16 reg_burst[] = {
    0x70cf, 0xffff, 0xc5d4, 0x903a, 0x2144, 0x0c00, 0x2186, 0x0ff3,
    0xb844, 0x262f, 0xf008, 0xb948, 0x21cc, 0x8021, 0xd801, 0xf203,
    0xd800, 0x7ee0, 0xc0f1, 0x71cf, 0xffff, 0xc610, 0x910e, 0x208c,
    0x8014, 0xf418, 0x910f, 0x208c, 0x800f, 0xf414, 0x9116, 0x208c,
    0x800a, 0xf410, 0x9117, 0x208c, 0x8807, 0xf40c, 0x9118, 0x2086,
    0x0ff3, 0xb848, 0x080d, 0x0090, 0xffea, 0xe081, 0xd801, 0xf203,
    0xd800, 0xc0d1, 0x7ee0, 0x78e0, 0xc0f1, 0x71cf, 0xffff, 0xc610,
    0x910e, 0x208c, 0x800a, 0xf418, 0x910f, 0x208c, 0x8807, 0xf414,
    0x9116, 0x208c, 0x800a, 0xf410, 0x9117, 0x208c, 0x8807, 0xf40c,
    0x9118, 0x2086, 0x0ff3, 0xb848, 0x080d, 0x0090, 0xffd9, 0xe080,
    0xd801, 0xf203, 0xd800, 0xf1df, 0x9040, 0x71cf, 0xffff, 0xc5d4,
    0xb15a, 0x9041, 0x73cf, 0xffff, 0xc7d0, 0xb140, 0x9042, 0xb141,
    0x9043, 0xb142, 0x9044, 0xb143, 0x9045, 0xb147, 0x9046, 0xb148,
    0x9047, 0xb14b, 0x9048, 0xb14c, 0x9049, 0x1958, 0x0084, 0x904a,
    0x195a, 0x0084, 0x8856, 0x1b36, 0x8082, 0x8857, 0x1b37, 0x8082,
    0x904c, 0x19a7, 0x009c, 0x881a, 0x7fe0, 0x1b54, 0x8002, 0x78e0,
    0x71cf, 0xffff, 0xc350, 0xd828, 0xa90b, 0x8100, 0x01c5, 0x0320,
    0xd900, 0x78e0, 0x220a, 0x1f80, 0xffff, 0xd4e0, 0xc0f1, 0x0811,
    0x0051, 0x2240, 0x1200, 0xffe1, 0xd801, 0xf006, 0x2240, 0x1900,
    0xffde, 0xd802, 0x1a05, 0x1002, 0xfff2, 0xf195, 0xc0f1, 0x0e7e,
    0x05c0, 0x75cf, 0xffff, 0xc84c, 0x9502, 0x77cf, 0xffff, 0xc344,
    0x2044, 0x008e, 0xb8a1, 0x0926, 0x03e0, 0xb502, 0x9502, 0x952e,
    0x7e05, 0xb5c2, 0x70cf, 0xffff, 0xc610, 0x099a, 0x04a0, 0xb026,
    0x0e02, 0x0560, 0xde00, 0x0a12, 0x0320, 0xb7c4, 0x0b36, 0x03a0,
    0x70c9, 0x9502, 0x7608, 0xb8a8, 0xb502, 0x70cf, 0x0000, 0x5536,
    0x7860, 0x2686, 0x1ffb, 0x9502, 0x78c5, 0x0631, 0x05e0, 0xb502,
    0x72cf, 0xffff, 0xc5d4, 0x923a, 0x73cf, 0xffff, 0xc7d0, 0xb020,
    0x9220, 0xb021, 0x9221, 0xb022, 0x9222, 0xb023, 0x9223, 0xb024,
    0x9227, 0xb025, 0x9228, 0xb026, 0x922b, 0xb027, 0x922c, 0xb028,
    0x1258, 0x0101, 0xb029, 0x125a, 0x0101, 0xb02a, 0x1336, 0x8081,
    0xa836, 0x1337, 0x8081, 0xa837, 0x12a7, 0x0701, 0xb02c, 0x1354,
    0x8081, 0x7fe0, 0xa83a, 0x78e0, 0xc0f1, 0x0dc2, 0x05c0, 0x7608,
    0x09bb, 0x0010, 0x75cf, 0xffff, 0xd4e0, 0x8d21, 0x8d00, 0x2153,
    0x0003, 0xb8c0, 0x8d45, 0x0b23, 0x0000, 0xea8f, 0x0915, 0x001e,
    0xff81, 0xe808, 0x2540, 0x1900, 0xffde, 0x8d00, 0xb880, 0xf004,
    0x8d00, 0xb8a0, 0xad00, 0x8d05, 0xe081, 0x20cc, 0x80a2, 0xdf00,
    0xf40a, 0x71cf, 0xffff, 0xc84c, 0x9102, 0x7708, 0xb8a6, 0x2786,
    0x1ffe, 0xb102, 0x0b42, 0x0180, 0x0e3e, 0x0180, 0x0f4a, 0x0160,
    0x70c9, 0x8d05, 0xe081, 0x20cc, 0x80a2, 0xf429, 0x76cf, 0xffff,
    0xc84c, 0x082d, 0x0051, 0x70cf, 0xffff, 0xc90c, 0x8805, 0x09b6,
    0x0360, 0xd908, 0x2099, 0x0802, 0x9634, 0xb503, 0x7902, 0x1523,
    0x1080, 0xb634, 0xe001, 0x1d23, 0x1002, 0xf00b, 0x9634, 0x9503,
    0x6038, 0xb614, 0x153f, 0x1080, 0xe001, 0x1d3f, 0x1002, 0xffa4,
    0x9602, 0x7f05, 0xd800, 0xb6e2, 0xad05, 0x0511, 0x05e0, 0xd800,
    0xc0f1, 0x0cfe, 0x05c0, 0x0a96, 0x05a0, 0x7608, 0x0c22, 0x0240,
    0xe080, 0x20ca, 0x0f82, 0x0000, 0x190b, 0x0c60, 0x05a2, 0x21ca,
    0x0022, 0x0c56, 0x0240, 0xe806, 0x0e0e, 0x0220, 0x70c9, 0xf048,
    0x0896, 0x0440, 0x0e96, 0x0400, 0x0966, 0x0380, 0x75cf, 0xffff,
    0xd4e0, 0x8d00, 0x084d, 0x001e, 0xff47, 0x080d, 0x0050, 0xff57,
    0x0841, 0x0051, 0x8d04, 0x9521, 0xe064, 0x790c, 0x702f, 0x0ce2,
    0x05e0, 0xd964, 0x72cf, 0xffff, 0xc700, 0x9235, 0x0811, 0x0043,
    0xff3d, 0x080d, 0x0051, 0xd801, 0xff77, 0xf025, 0x9501, 0x9235,
    0x0911, 0x0003, 0xff49, 0x080d, 0x0051, 0xd800, 0xff72, 0xf01b,
    0x0886, 0x03e0, 0xd801, 0x0ef6, 0x03c0, 0x0f52, 0x0340, 0x0dba,
    0x0200, 0x0af6, 0x0440, 0x0c22, 0x0400, 0x0d72, 0x0440, 0x0dc2,
    0x0200, 0x0972, 0x0440, 0x0d3a, 0x0220, 0xd820, 0x0bfa, 0x0260,
    0x70c9, 0x0451, 0x05c0, 0x78e0, 0xd900, 0xf00a, 0x70cf, 0xffff,
    0xd520, 0x7835, 0x8041, 0x8000, 0xe102, 0xa040, 0x09f1, 0x8114,
    0x71cf, 0xffff, 0xd4e0, 0x70cf, 0xffff, 0xc594, 0xb03a, 0x7fe0,
    0xd800, 0x0000, 0x0000, 0x0500, 0x0500, 0x0200, 0x0330, 0x0000,
    0x0000, 0x03cd, 0x050d, 0x01c5, 0x03b3, 0x00e0, 0x01e3, 0x0280,
    0x01e0, 0x0109, 0x0080, 0x0500, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0xffff, 0xc9b4, 0xffff, 0xd324, 0xffff, 0xca34,
    0xffff, 0xd3ec
  };

  mt9m114_write(sd, REG_ACCESS_CTL_STAT, 2, 0x0001);
  mt9m114_write(sd, REG_PHYSICAL_ADDRESS_ACCESS, 2, 0x512c);
  mt9m114_burst_write(sd,0xd12c, reg_burst, ARRAY_SIZE(reg_burst));

  mt9m114_write(sd, REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000);
  mt9m114_write(sd, REG_PATCHLDR_LOADER_ADDRESS, 2, 0x04b4);
  mt9m114_write(sd, REG_PATCHLDR_PATCH_ID, 2, 0x0302);
  mt9m114_write(sd, REG_PATCHLDR_FIRMWARE_ID, 4, 0x41030202);

  v = HOST_COMMAND_APPLY_PATCH | HOST_COMMAND_OK;
  mt9m114_write(sd, REG_COMMAND_REGISTER, 2, v);

  if(poll_command_register_bit(sd,HOST_COMMAND_APPLY_PATCH))
  {
    dprintk(0,"MT9M114","MT9M114 : poll apply patch timeout\n");
  }

  mt9m114_read(sd, REG_COMMAND_REGISTER, 2, &v);
  if ( !(v & HOST_COMMAND_OK))
  {
    dprintk(0,"MT9M114","Warning : apply patch 3 Adaptive Sensitivity Host_command not OK\n");
    return -1;
  }

  mt9m114_read(sd, REG_PATCHLDR_APPLY_STATUS, 1, &v);
  if (v)
  {
    dprintk(0,"MT9M114","MT9M114 : patch apply 3 Adaptive Sensitivity status non-zero - value:%x\n",v);
    return -1;
  }

  return 0;
}

/**
 * Sets the specified uvc register to synced.
 * @param sd
 * @param reg
 */
static void mt9m114_set_uvc_register_synced(struct v4l2_subdev *sd, uvc_registers reg)
{
  struct mt9m114_info *info = to_state(sd);
  info->uvc_register_out_of_sync[reg] = false;   
}

/**
 * Checks whether the specified uvc register is currently synced.
 * @param sd
 * @param reg
 * @return  true if it is synced.
 */
static bool mt9m114_uvc_register_is_out_of_sync(struct v4l2_subdev *sd, uvc_registers reg)
{
  struct mt9m114_info *info = to_state(sd);
  return info->uvc_register_out_of_sync[reg];
}

static int mt9m114_set_state_command(struct v4l2_subdev *sd)
{
  //NOTE: code from this method has been copied to wait_for_end_of_frame().
  //      If you fix a bug in here, fix it over there as well.
  u32 v=0;
  // (Optional) First check that the FW is ready to accept a new command
  mt9m114_read(sd, REG_COMMAND_REGISTER, 2, &v);
  if (v & HOST_COMMAND_SET_STATE)
  {
    dprintk(0,"MT9M114","MT9M114 : Set State cmd bit is already set 0x%x\n",v);
    return -1;
  }
  // (Mandatory) Issue the Set State command
  // We set the 'OK' bit so we can detect if the command fails
  mt9m114_write(sd, REG_COMMAND_REGISTER, 2, HOST_COMMAND_SET_STATE|HOST_COMMAND_OK);
  // Wait for the FW to complete the command (clear the HOST_COMMAND_1 bit)
  poll_command_register_bit(sd,HOST_COMMAND_SET_STATE);
  // Check the 'OK' bit to see if the command was successful
  mt9m114_read(sd, REG_COMMAND_REGISTER, 2, &v);
  if ( !(v & HOST_COMMAND_OK))
  {
    dprintk(0,"MT9M114","MT9M114 : set state command fail");
    return -1;
  }
  return 0;
}


/**
 * Refresh subsystems without requiring a sensor configuration change.
 * @note This call blocks till the next frame.
 * @param sd
 * @return 
 */
static int mt9m114_refresh(struct v4l2_subdev *sd)
{
  u32 v=0;//a temporary variable used for several read commands
  
  //make sure that the refresh command is really processed and that 
  //exposure and user changes are processed as well.
  mt9m114_read(sd, REG_UVC_ALGO, 2, &v);
  v |= 0b111;
  mt9m114_write(sd, REG_UVC_ALGO,2, v);
  //Changes to REG_UVC_ALGO take effect on vertical blanking, therefore wait one frame.
  mt9m114_wait_num_frames(sd, 1);
  
  // First check that the FW is ready to accept a Refresh command
  mt9m114_read(sd, REG_COMMAND_REGISTER, 2, &v);
  if (v & HOST_COMMAND_REFRESH)
  {
    dprintk(0,"MT9M114","MT9M114 : Refresh cmd bit is already set 0x%x\n",v);
    return -1;
  }

  // Issue the Refresh command, and set the 'OK' bit at the time time so
  //we can detect if the command fails
  mt9m114_write(sd, REG_COMMAND_REGISTER, 2, HOST_COMMAND_REFRESH|HOST_COMMAND_OK);

  // Wait for the FW to complete the command
  poll_command_register_bit(sd,HOST_COMMAND_REFRESH);

  // Check the 'OK' bit to see if the command was successful
  mt9m114_read(sd, REG_COMMAND_REGISTER, 2, &v);
  if ( !(v & HOST_COMMAND_OK))
  {
    dprintk(0,"MT9M114","MT9M114 : refresh command fail");
    return -1;
  }

  //check refresh command error code
  mt9m114_read(sd, REG_SEQ_ERROR_CODE, 1, &v);
  if(v != 0)
  {
    dprintk(0,"MT9M114","%s Refresh ERROR: %x\n",__func__, v);
  }
  
  
  mt9m114_read(sd, REG_UVC_RESULT_STATUS, 1, &v);
  dprintk(0,"MT9M114","%s REG_UVC_RESULT_STATUS: %x\n",__func__, v);

  
  //the refresh command schedules an update on the next end of frame.
  //It does not wait until the end of frame is actually reached.
  //Therefore we need to wait until the end of the frame manually.
  mt9m114_wait_num_frames(sd, 1);
  
  
  return 0;
}

/**
 * Waits until a number of frames have passed
 * This method can be used to wait for vertical blanking as vertical blanking 
 * occurs at the end of a frame.
 * @param sd
 * @param numFrames the number of frames to wait
 * @return -1 in case of error, 0 otherwise.
 */
static int mt9m114_wait_num_frames(struct v4l2_subdev *sd, u16 numFrames)
{
    //FIXME copy & paste from set_state_command
    u32 v=0;
    u32 frameCountBefore = 0;
    u32 frameCountAfter = 0;
    
    //get the current frame count
    mt9m114_read(sd, CAM_MON_HEARTBEAT, 2, &frameCountBefore);
    
    
    //specify for which event we want to wait:  2 = start of next frame
    mt9m114_write(sd, REG_CMD_HANDLER_WAIT_FOR_EVENT, 2, 2);

    //specify for how much frames we want to wait
    mt9m114_write(sd, REG_CMD_HANDLER_NUM_WAIT_EVENTS, 2, numFrames);
   
    // (Optional) First check that the FW is ready to accept a new command
    mt9m114_read(sd, REG_COMMAND_REGISTER, 2, &v);
    if (v & HOST_COMMAND_WAIT_FOR_EVENT)
    {
      //This should never happen as long as nobody opens the driver device in async mode.
      dprintk(0,"MT9M114","MT9M114 : Host command wait for event already set 0x%x\n",v);
      return -1;
    }
    // (Mandatory) Issue the wait for command
    // We set the 'OK' bit so we can detect if the command fails. The chip will unset the OK bit if everything is ok.
    mt9m114_write(sd, REG_COMMAND_REGISTER, 2, HOST_COMMAND_WAIT_FOR_EVENT | HOST_COMMAND_OK);
    // Wait for the FW to complete the command (clear the HOST_COMMAND_1 bit)
    poll_command_register_bit(sd, HOST_COMMAND_WAIT_FOR_EVENT);
    // Check the 'OK' bit to see if the command was successful
    mt9m114_read(sd, REG_COMMAND_REGISTER, 2, &v);
    if ( !(v & HOST_COMMAND_OK))
    {
      dprintk(0,"MT9M114","MT9M114 : wait for end of frame failed: TIMEOUT?!");
      return -1;
    }
    
    //read frame count after
    mt9m114_read(sd, CAM_MON_HEARTBEAT, 2, &frameCountAfter);
    
    if(frameCountBefore == frameCountAfter)
    {
      dprintk(0,"MT9M114","MT9M114 : wait for end of frame failed. Frame is still the same.");
    }
    
    return 0;
}

/**
 * Indicate that uvc and cam variables have gone out of sync.
 */
static void mt9m114_uvc_out_of_sync(struct mt9m114_info* pInfo) 
{

    int i;
    for(i = 0; i < NUM_OF_UVC_REGISTERS; ++i)
    {
        pInfo->uvc_register_out_of_sync[i] = true;
    }
}

/**
 * Re-configure device state using CAM configuration variables.
 * @param sd
 * @return 0 on success or a negative error code.
 */
static int mt9m114_change_config(struct v4l2_subdev *sd)
{
  u32 v=0;
  int ret;
  struct mt9m114_info *info = to_state(sd);

  /*
   * change_config updates the sensor configuration using the cam variables.
   * It totally ignores what the uvc variables say.
   * E.g. if cam says the exposure should be 20 then after change_config the
   * exposure will be 20 even though uvc says it should be 42.
   * 
   * This is not a problem in itself.
   * However the uvc variables are not updated.
   * After a change config command the uvc will still tell you that the exposure
   * is 42. Even though it really is 20.
   * Additionally if the user tries to reset the exposure to 42
   * using uvc does not work because from uvc's point of view the variable has not
   * changed.
   * 
   */
  mt9m114_uvc_out_of_sync(info); //after change_config all uvc registers will be out of sync.
  
  /* Program orientation register. */
  mt9m114_write(sd, REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000);
  ret = mt9m114_read(sd, REG_CAM_SENSOR_CONTROL_READ_MODE, 2, &v);


  if (info->flag_vflip)
    v |= CAM_SENSOR_CONTROL_VERT_FLIP_EN;
  else
    v &= ~CAM_SENSOR_CONTROL_VERT_FLIP_EN;

  if (info->flag_hflip)
    v |= CAM_SENSOR_CONTROL_HORZ_FLIP_EN;
  else
    v &= ~CAM_SENSOR_CONTROL_HORZ_FLIP_EN;

  mt9m114_write(sd, REG_CAM_SENSOR_CONTROL_READ_MODE, 2, v);

  // Set the desired next state (SYS_STATE_ENTER_CONFIG_CHANGE = 0x28)
  mt9m114_write(sd,REG_SYSMGR_NEXT_STATE, 1, MT9M114_SYS_STATE_ENTER_CONFIG_CHANGE);
  ret = mt9m114_set_state_command(sd);
  if(ret < 0)
    return ret;
  mt9m114_read(sd, REG_SYSMGR_CURRENT_STATE, 1, &v);
  if(v!=MT9M114_SYS_STATE_STREAMING)
  {
    dprintk(0,"MT9M114","MT9M114 %s System state is not STREAMING\n",__func__);
    return -1;
  }

  mt9m114_read(sd, REG_UVC_RESULT_STATUS, 1, &v);
  dprintk(0,"MT9M114","%s REG_UVC_RESULT_STATUS:%x\n",__func__, v);
  
  return 0;
}

static int mt9m114_sensor_optimization(struct v4l2_subdev *sd)
{
  //FIXME all registers used in here are undocumented. No idea about their purpose.
  //      Maybe some commands for the fpga?
  mt9m114_write(sd, REG_DAC_TXLO_ROW, 2, 0x8270); 
  mt9m114_write(sd, REG_DAC_TXLO, 2, 0x8270); 
  mt9m114_write(sd, REG_DAC_LD_4_5, 2, 0x3605); 
  mt9m114_write(sd, REG_DAC_LD_6_7, 2, 0x77FF); 
  mt9m114_write(sd, REG_DAC_ECL, 2, 0xC233); 
  mt9m114_write(sd, REG_DELTA_DK_CONTROL, 2, 0x87FF);
  mt9m114_write(sd, REG_COLUMN_CORRECTION, 2, 0x6080); 
  mt9m114_write(sd, REG_AE_TRACK_MODE, 2, 0x0008); 
  return 0;
}

static int mt9m114_reset(struct v4l2_subdev *sd, u32 val)
{
  u32 v;
  printk(KERN_INFO "[%s:%u]\n",__FUNCTION__,__LINE__);
  dprintk(0,"MT9M114","MT9M114 : Resetting chip!\n");
  mt9m114_read(sd, REG_RESET_AND_MISC_CONTROL, 2, &v);
  mt9m114_write(sd, REG_RESET_AND_MISC_CONTROL, 2, v|0x01);
  msleep(100); //FIXME This sleep shouldn't be here according to the documentation
  mt9m114_write(sd, REG_RESET_AND_MISC_CONTROL, 2, v & (~1));
  msleep(100); //datasheet documentation
  mt9m114_errata_2(sd);
  return 0;
}

static int mt9m114_PLL_settings(struct v4l2_subdev *sd)
{
  mt9m114_write(sd,REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000);
  mt9m114_write(sd,REG_CAM_SYSCTL_PLL_ENABLE, 1, 1); //no idea what it does. 1 is default
  mt9m114_write(sd,REG_CAM_SYSCTL_PLL_DIVIDER_M_N, 2, 0x0120); //no idea, default is 0x09a0
  mt9m114_write(sd,REG_CAM_SYSCTL_PLL_DIVIDER_P, 2, 0x0700);//no idea, default is 0x0700
  return 0;
}

static int mt9m114_CPIPE_preference(struct v4l2_subdev *sd)
{
  mt9m114_write(sd, REG_CAM_LL_START_BRIGHTNESS, 2, 0x0020);
  mt9m114_write(sd, REG_CAM_LL_STOP_BRIGHTNESS, 2, 0x009A);
  mt9m114_write(sd, REG_CAM_LL_START_GAIN_METRIC, 2, 0x0070);
  mt9m114_write(sd, REG_CAM_LL_STOP_GAIN_METRIC, 2, 0x00F3);

  mt9m114_write(sd, REG_CAM_LL_START_TARGET_LUMA_BM, 2, 0x0020);
  mt9m114_write(sd, REG_CAM_LL_STOP_TARGET_LUMA_BM, 2, 0x009A);

  mt9m114_write(sd, REG_CAM_LL_START_SATURATION, 1, 0x80);
  mt9m114_write(sd, REG_CAM_LL_END_SATURATION, 1, 0x4B);
  mt9m114_write(sd, REG_CAM_LL_START_DESATURATION, 1, 0x00);
  mt9m114_write(sd, REG_CAM_LL_END_DESATURATION, 1, 0xFF);

  mt9m114_write(sd, REG_CAM_LL_START_DEMOSAIC, 1, 0x1E);
  mt9m114_write(sd, REG_CAM_LL_START_AP_GAIN, 1, 0x02);
  mt9m114_write(sd, REG_CAM_LL_START_AP_THRESH, 1, 0x06);
  mt9m114_write(sd, REG_CAM_LL_STOP_DEMOSAIC, 1, 0x3C);
  mt9m114_write(sd, REG_CAM_LL_STOP_AP_GAIN, 1, 0x01);
  mt9m114_write(sd, REG_CAM_LL_STOP_AP_THRESH, 1, 0x0C);

  mt9m114_write(sd, REG_CAM_LL_START_NR_RED, 1, 0x3C);
  mt9m114_write(sd, REG_CAM_LL_START_NR_GREEN, 1, 0x3C);
  mt9m114_write(sd, REG_CAM_LL_START_NR_BLUE, 1, 0x3C);
  mt9m114_write(sd, REG_CAM_LL_START_NR_THRESH, 1, 0x0F);
  mt9m114_write(sd, REG_CAM_LL_STOP_NR_RED, 1, 0x64);
  mt9m114_write(sd, REG_CAM_LL_STOP_NR_GREEN, 1, 0x64);
  mt9m114_write(sd, REG_CAM_LL_STOP_NR_BLUE, 1, 0x64);
  mt9m114_write(sd, REG_CAM_LL_STOP_NR_THRESH, 1, 0x32);

  mt9m114_write(sd, REG_CAM_LL_START_CONTRAST_BM, 2, 0x0020);
  mt9m114_write(sd, REG_CAM_LL_STOP_CONTRAST_BM, 2, 0x009A);
  mt9m114_write(sd, REG_CAM_LL_GAMMA, 2, 0x00DC);
  mt9m114_write(sd, REG_CAM_LL_START_CONTRAST_GRADIENT, 1, 0x38);
  mt9m114_write(sd, REG_CAM_LL_STOP_CONTRAST_GRADIENT, 1, 0x30);
  mt9m114_write(sd, REG_CAM_LL_START_CONTRAST_LUMA_PERCENTAGE, 1, 0x50);
  mt9m114_write(sd, REG_CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE, 1, 0x19);

  mt9m114_write(sd, REG_CAM_LL_START_FADE_TO_BLACK_LUMA, 2, 0x0230);
  mt9m114_write(sd, REG_CAM_LL_STOP_FADE_TO_BLACK_LUMA, 2, 0x0010);

  mt9m114_write(sd, REG_CAM_LL_CLUSTER_DC_TH_BM, 2, 0x0800);

  mt9m114_write(sd, REG_CAM_LL_CLUSTER_DC_GATE_PERCENTAGE, 1, 0x05);

  mt9m114_write(sd, REG_CAM_LL_SUMMING_SENSITIVITY_FACTOR, 1, 0x40);

  mt9m114_write(sd, REG_CAM_AET_TARGET_AVERAGE_LUMA_DARK, 1, 0x1B);

  mt9m114_write(sd, REG_CAM_AET_AEMODE, 1, 0x0E);
  mt9m114_write(sd, REG_CAM_AET_TARGET_GAIN, 2, 0x0080);
  mt9m114_write(sd, REG_CAM_AET_AE_MAX_VIRT_AGAIN, 2, 0x0100);
  mt9m114_write(sd, REG_CAM_SENSOR_CFG_MAX_ANALOG_GAIN, 2, 0x01F8);

  mt9m114_write(sd, REG_CAM_AET_BLACK_CLIPPING_TARGET, 2, 0x005A);

  mt9m114_write(sd, REG_CCM_DELTA_GAIN, 1, 0x05);
  mt9m114_write(sd, REG_AE_TRACK_AE_TRACKING_DAMPENING_SPEED, 1, 0x20);
  return 0;
}

static int mt9m114_features(struct v4l2_subdev *sd)
{
  mt9m114_write(sd, REG_LOGICAL_ADDRESS_ACCESS, 2, 0x0000);
  mt9m114_write(sd, REG_CAM_PORT_OUTPUT_CONTROL, 2, 0x8040);
  mt9m114_write(sd, REG_PAD_SLEW_CONTROL,2 , 0x0777);
  mt9m114_write_array(sd, mt9m114_fmt_yuv422);

  mt9m114_write(sd, REG_UVC_ALGO,2 , 0x07);
  mt9m114_write(sd, REG_UVC_FRAME_INTERVAL, 4, 0x1e00);

  return 0;
}

static int mt9m114_init(struct v4l2_subdev *sd,u32 val)
{
  int ret = 0;

  printk(KERN_INFO "[%s:%u]\n",__FUNCTION__,__LINE__);

  ret += mt9m114_reset(sd,0);
  ret += mt9m114_PLL_settings(sd);
  ret += mt9m114_write_array(sd, mt9m114_vga_30_scaling_regs);
  ret += mt9m114_sensor_optimization(sd);
  ret += mt9m114_errata_1(sd);
  ret += mt9m114_errata_2(sd);
  ret += mt9m114_write_array(sd, pga_regs);
  ret += mt9m114_write_array(sd, ccm_awb_regs);
  ret += mt9m114_CPIPE_preference(sd);
  ret += mt9m114_features(sd);
  ret += mt9m114_write_array(sd, uvc_ctrl_regs);
  ret += mt9m114_change_config(sd);
  ret += mt9m114_patch2_black_lvl_correction_fix(sd);
  ret += mt9m114_patch3_adaptive_sensitivity(sd);
  if (ret != 0)
  {
    dprintk(0,"MT9M114","MT9M114 : init fail\n");
  }
  return ret;
}

static int mt9m114_detect(struct v4l2_subdev *sd)
{
  u32 chip_id;
  u32 mon_major_version;
  u32 mon_minor_version;
  u32 mon_release_version;
  int ret;

  ret = mt9m114_read(sd, REG_CHIP_ID, 2, &chip_id);
  ret = mt9m114_read(sd, REG_MON_MAJOR_VERSION, 2, &mon_major_version);
  ret = mt9m114_read(sd, REG_MON_MINOR_VERION, 2, &mon_minor_version);
  ret = mt9m114_read(sd, REG_MON_RELEASE_VERSION, 2, &mon_release_version);

  if (ret < 0)
    return ret;

  if(chip_id!=0)
    dprintk(0,"MT9M114","MT9M114 found : chip_id:%x major:%x minor:%x release:%x", chip_id,mon_major_version,mon_minor_version,mon_release_version);

  if (chip_id != 0x2481) /* default chipid*/
    return -ENODEV;

  // mt9m114 found, init it
  ret = mt9m114_init(sd,0);

  return ret;
}

/**
 * Syncs the specified register and sets its value.
 * @param sd
 * @param reg
 * @param size
 * @param value
 * @return 0 on success. error code otherwise.
 */
static int mt9m114_sync_and_set_uvc_register_u32(struct v4l2_subdev *sd, u16 reg, u32 size, u32 value)
{
    u32 oldValue;
    int ret = 0;
    
    ret = mt9m114_read(sd, reg, size, &oldValue);
    if(0 != ret) return ret;
    
    if(oldValue == value)
    { //we only need to sync if the value is exactly the same.
      //if it is not the same the sync will happen automatically  
     
      //+1 -1 is done because the value might already be at it's maximum or minimum.
      //If it is at maximum +1 wouldn't work, if it is at minimum -1 would't work
      ret = mt9m114_write(sd, reg, size, oldValue + 1);
      if(0 != ret) return ret;
      ret = mt9m114_write(sd, reg, size, oldValue - 1); 
      if(0 != ret) return ret;
    }
    //write new value
    ret = mt9m114_write(sd, reg, size, value);  
    return ret;
}

static int mt9m114_sync_and_set_uvc_register_s32(struct v4l2_subdev *sd, u16 reg, u32 size, s32 value)
{
  //This is copy & paste from mt9m114_sync_and_set_uvc_register_u32
    s32 oldValue;
    int ret = 0;
    
    ret = mt9m114_read(sd, reg, size, &oldValue);
    if(0 != ret) return ret;
    
    if(oldValue == value)
    { //we only need to sync if the value is exactly the same.
      //if it is not the same the sync will happen automatically  
     
      //+1 -1 is done because the value might already be at it's maximum or minimum.
      //If it is at maximum +1 wouldn't work, if it is at minimum -1 would't work
      ret = mt9m114_write(sd, reg, size, oldValue + 1);
      if(0 != ret) return ret;
      ret = mt9m114_write(sd, reg, size, oldValue - 1); 
      if(0 != ret) return ret;
    }
    //write new value
    ret = mt9m114_write(sd, reg, size, value);  
    return ret;   
}


/*
 * Store information about the video data format.  The color matrix
 * is deeply tied into the format, so keep the relevant values here.
 * The magic matrix nubmers come from OmniVision.
 */
static struct mt9m114_format_struct {
  u8 *desc;
  u32 pixelformat;
  enum v4l2_mbus_pixelcode code;
  enum v4l2_colorspace colorspace;
  struct regval_list *regs;
  int bpp;   /* Bytes per pixel */
} mt9m114_formats[] = {
  {
    .desc			= "YUYV 4:2:2",
    .pixelformat	= V4L2_PIX_FMT_YUYV,
    .code			= V4L2_MBUS_FMT_YUYV8_2X8,
    .colorspace 	= V4L2_COLORSPACE_JPEG,
    .regs 			= mt9m114_fmt_yuv422,
    .bpp			= 2,
  },
};
#define N_MT9M114_FMTS ARRAY_SIZE(mt9m114_formats)


/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct mt9m114_win_size {
  int	width;
  int	height;
  int ll_corection;
  int   binned;
  struct regval_list *regs; /* Regs to tweak */
/* h/vref stuff */
} mt9m114_win_sizes[] = {

  /* 960p@28fps */
  {
     .width		    = WXGA_WIDTH,
     .height		    = FULL_HEIGHT,
     .ll_corection          = 0,
     .binned                = 0,
     .regs 		    = mt9m114_960p30_regs,
  },
  /* 720p@36fps */
  {
    .width		    = WXGA_WIDTH,
    .height		    = WXGA_HEIGHT,
    .ll_corection           = 0,
    .binned                 = 0,
    .regs 		    = mt9m114_720p36_regs,
  },
  /* VGA@30fps scaling*/
  {
    .width                  = VGA_WIDTH,
    .height                 = VGA_HEIGHT,
    .ll_corection           = 0,
    .binned                 = 0,
    .regs                   = mt9m114_vga_30_scaling_regs,
  },
  /* QVGA@30fps scaling*/
  {
    .width                  = QVGA_WIDTH,
    .height                 = QVGA_HEIGHT,
    .ll_corection           = 1,
    .binned                 = 0,
    .regs                   = mt9m114_qvga_30_scaling_regs,
  },
  /* 160x120@30fps scaling*/
  {
    .width                  = 160,
    .height                 = 120,
    .ll_corection           = 1,
    .binned                 = 0,
    .regs                   = mt9m114_160x120_30_scaling_regs,
  },
#if 0
  /* VGA@30fps to 75fps binned*/
  {
    .width		    = VGA_WIDTH,
    .height		    = VGA_HEIGHT,
    .ll_corection           = 0,
    .binned                 = 1,
    .regs 		    = mt9m114_vga_30_to_75_binned_regs,
  },
  /* QVGA@30fps to 120fps binned*/
  {
    .width		    = QVGA_WIDTH,
    .height		    = QVGA_HEIGHT,
    .ll_corection           = 1,
    .binned                 = 1,
    .regs 		    = mt9m114_qvga_30_to_120_binned_regs,
  },
  /* 160x120@30fps to 120fps binned*/
  {
    .width                  = 160,
    .height                 = 120,
    .ll_corection           = 1,
    .binned                 = 1,
    .regs                   = mt9m114_160x120_30_to_120_binned_regs,
  },
#endif
};

#define N_WIN_SIZES (ARRAY_SIZE(mt9m114_win_sizes))


static int mt9m114_enum_fmt(struct v4l2_subdev *sd, unsigned int index, enum v4l2_mbus_pixelcode *code)
{
  printk(KERN_INFO "[%s:%u]\n",__FUNCTION__,__LINE__);

//  struct mt9m114_format_struct *ofmt;

  if (index >= N_MT9M114_FMTS)
    return -EINVAL;

  //ofmt = mt9m114_formats + index;
  //strcpy(fmt->description, ofmt->desc);
  *code = V4L2_MBUS_FMT_YUYV8_2X8;//ofmt->pixelformat;
  return 0;
}


static int mt9m114_try_fmt_internal(struct v4l2_subdev *sd,
    struct v4l2_mbus_framefmt *fmt,
    struct mt9m114_format_struct **ret_fmt,
    struct mt9m114_win_size **ret_wsize)
{
  int index;
  struct mt9m114_win_size *wsize;

  printk(KERN_INFO "[%s:%u]\n",__FUNCTION__,__LINE__);

  //struct v4l2_pix_format *pix = &fmt->fmt.pix;

  for (index = 0; index < N_MT9M114_FMTS; index++)
    if (mt9m114_formats[index].code == fmt->code) {
      fmt->colorspace = mt9m114_formats[index].colorspace;
      break;
    }

  if (index >= N_MT9M114_FMTS) {
    /* default to first format */
    index = 0;
    fmt->code = mt9m114_formats[0].code;
    fmt->colorspace = mt9m114_formats[0].colorspace;
  }
  if (ret_fmt != NULL)
    *ret_fmt = mt9m114_formats + index;

  fmt->field = V4L2_FIELD_NONE;
  /*
   * Round requested image size down to the nearest
   * we support, but not below the smallest.
   */
  for (wsize = mt9m114_win_sizes; wsize < mt9m114_win_sizes + N_WIN_SIZES;
       wsize++)
    if (fmt->width >= wsize->width && fmt->height >= wsize->height)
      break;
  if (wsize >= mt9m114_win_sizes + N_WIN_SIZES)
    wsize--;   /* Take the smallest one */
  if (ret_wsize != NULL)
    *ret_wsize = wsize;
  /*
   * Note the size we'll actually handle.
   */
  fmt->width = wsize->width;
  fmt->height = wsize->height;
//  pix->bytesperline = pix->width*mt9m114_formats[index].bpp;
//  pix->sizeimage = pix->height*pix->bytesperline;
  return 0;
}

static int mt9m114_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
  printk(KERN_INFO "[%s:%u]\n",__FUNCTION__,__LINE__);

  return mt9m114_try_fmt_internal(sd, fmt, NULL, NULL);
}

/*
 * Set a format.
 */
static int mt9m114_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
  int ret;
  u32 v;
  struct mt9m114_format_struct *mtfmt;
  struct mt9m114_win_size *wsize;
  struct mt9m114_info *info = to_state(sd);

  printk(KERN_INFO "[%s:%u]\n",__FUNCTION__,__LINE__);

  ret = mt9m114_try_fmt_internal(sd, fmt, &mtfmt, &wsize);
  if (ret)
    return ret;

  mt9m114_write_array(sd, mtfmt->regs);
  ret = 0;
  if (wsize->regs)
    ret = mt9m114_write_array(sd, wsize->regs);
  info->fmt = mtfmt;
  mt9m114_read(sd, REG_LL_ALGO, 2, &v);
  if (wsize->ll_corection)
    v = v | LL_EXEC_DELTA_DK_CORRECTION;
  else
    v = v & (~LL_EXEC_DELTA_DK_CORRECTION);
  mt9m114_write(sd, REG_LL_ALGO, 2, v);
  mt9m114_change_config(sd);

  mt9m114_read(sd, REG_CAM_SENSOR_CONTROL_READ_MODE, 2, &v);
  if (wsize->binned)
      v = v | CAM_SENSOR_CONTROL_BINNING_EN;
  else
      v = v & (~CAM_SENSOR_CONTROL_BINNING_EN);
  mt9m114_write(sd, REG_CAM_SENSOR_CONTROL_READ_MODE, 2, v);
  mt9m114_change_config(sd);

  return ret;
}

/*
 * Implement G/S_PARM.  There is a variable framerate available
 * to do someday;
 */
static int mt9m114_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
  struct v4l2_captureparm *cp = &parms->parm.capture;
  u32 v=0;
  int ret=0;


  printk(KERN_INFO "[%s:%u]\n",__FUNCTION__,__LINE__);

  if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
    return -EINVAL;

  memset(cp, 0, sizeof(struct v4l2_captureparm));
  cp->capability = V4L2_CAP_TIMEPERFRAME;

  mt9m114_read(sd, REG_CAM_AET_MAX_FRAME_RATE, 2, &v);

  cp->timeperframe.numerator = 1;
  cp->timeperframe.denominator = v/MT9M114_FRAME_RATE;
  return ret;
}

static int mt9m114_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{

  struct v4l2_captureparm *cp = &parms->parm.capture;
  struct v4l2_fract *tpf = &cp->timeperframe;
  int div;

  if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
    return -EINVAL;
  if (cp->extendedmode != 0)
    return -EINVAL;

  if (tpf->numerator == 0 || tpf->denominator == 0)
    div = MT9M114_FRAME_RATE * 30;  /* Reset to full rate */
  else
    div = MT9M114_FRAME_RATE * tpf->denominator / tpf->numerator;
  if (div == 0)
    div =MT9M114_FRAME_RATE * 30;  /* Reset to full rate */

  tpf->numerator = 1;
  tpf->denominator = div/MT9M114_FRAME_RATE;
  mt9m114_write(sd, REG_CAM_AET_MAX_FRAME_RATE, 2, div);
  mt9m114_write(sd, REG_CAM_AET_MIN_FRAME_RATE, 2, div);
  mt9m114_change_config(sd);

  return 0;
}


static int mt9m114_s_sat(struct v4l2_subdev *sd, s32 value)
{
  int ret = 0;
  //if this uvc register is out of sync
  if(mt9m114_uvc_register_is_out_of_sync(sd, UVC_SATURATION))
  {//sync it
    dprintk(0,"MT9M114","MT9M114 :REG_UVC_SATURATION out of sync. syncing...\n"); 
    ret = mt9m114_sync_and_set_uvc_register_s32(sd, REG_UVC_SATURATION, 2, value);
    mt9m114_set_uvc_register_synced(sd, UVC_SATURATION);
  }
  else
  {//if the register is not out of sync: just set the value
    ret = mt9m114_write(sd, REG_UVC_SATURATION, 2, value); 
  }
  
  if(0 != ret)
  {
    dprintk(0,"MT9M114","MT9M114 : %s error writing value. errno: 0x%x\n", __func__, ret);
    return ret;
  }
  
  ret = check_uvc_status(sd, __func__);
  if(0 != ret)
  { //error has already been printed by check_uvc_status.
    return ret;
  }
  
  mt9m114_refresh(sd);

  return ret;
  

}

static int mt9m114_g_sat(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_UVC_SATURATION, 2, &v);

  *value = v;
  return ret;
}


static int mt9m114_s_hue(struct v4l2_subdev *sd, int value)
{
  int ret;

  ret = mt9m114_write(sd, REG_CAM_HUE_ANGLE, 1, (s8)value); //s8 cast is important to keep the sign
  mt9m114_refresh(sd);
  
  return ret;
}

static int mt9m114_g_hue(struct v4l2_subdev *sd, s32 *value)
{
  //FIXME there is a bug somewhere. If you set hue to -22 g_hue will return -9.
  //      -21 will return -8 etc.
  //      22 and 21 will return 8. 20 7 etc.
  //      There seems to be some strange internal clamping?!
  s8 v = 0;
  int ret = mt9m114_read(sd, REG_UVC_HUE, 1, (u32*)&v);
  *value = v;
  return ret;
}


static int mt9m114_s_brightness(struct v4l2_subdev *sd, int value)
{
  int ret = 0;
  s32 shiftedValue = value>>2;
  //if this uvc register is out of sync
  if(mt9m114_uvc_register_is_out_of_sync(sd, UVC_BRIGHTNESS))
  {//sync it
    dprintk(0,"MT9M114","MT9M114 :REG_UVC_BRIGHTNESS out of sync. syncing...\n"); 
    ret = mt9m114_sync_and_set_uvc_register_s32(sd, REG_UVC_BRIGHTNESS, 2, shiftedValue);
    mt9m114_set_uvc_register_synced(sd, UVC_BRIGHTNESS);
  }
  else
  {//if the register is not out of sync: just set the value
    ret = mt9m114_write(sd, REG_UVC_BRIGHTNESS, 2, shiftedValue); 
  }
  
  if(0 != ret)
  {
    dprintk(0,"MT9M114","MT9M114 : %s error writing value. errno: 0x%x\n", __func__, ret);
    return ret;
  }
  
  ret = check_uvc_status(sd, __func__);
  if(0 != ret)
  { //error has already been printed by check_uvc_status.
    return ret;
  }
  
  mt9m114_refresh(sd);

  return ret;
  
}

static int mt9m114_g_brightness(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_UVC_BRIGHTNESS, 2, &v);

  *value = v<<2;
  return ret;
}


static int mt9m114_s_contrast(struct v4l2_subdev *sd, int value)
{
  int ret = 0;
  //if this uvc register is out of sync
  if(mt9m114_uvc_register_is_out_of_sync(sd, UVC_CONTRAST))
  {//sync it
    dprintk(0,"MT9M114","MT9M114 :REG_UVC_CONTRAST out of sync. syncing...\n"); 
    ret = mt9m114_sync_and_set_uvc_register_s32(sd, REG_UVC_CONTRAST, 2, value);
    mt9m114_set_uvc_register_synced(sd, UVC_CONTRAST);
  }
  else
  {//if the register is not out of sync: just set the value
    ret = mt9m114_write(sd, REG_UVC_CONTRAST, 2, value); 
  }
  
  if(0 != ret)
  {
    dprintk(0,"MT9M114","MT9M114 : %s error writing value. errno: 0x%x\n", __func__, ret);
    return ret;
  }
  
  ret = check_uvc_status(sd, __func__);
  if(0 != ret)
  { //error has already been printed by check_uvc_status.
    return ret;
  }
  
  mt9m114_refresh(sd);

  return ret;
  
}

static int mt9m114_g_contrast(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_UVC_CONTRAST, 2, &v);

  *value = v;
  return ret;
}


static int mt9m114_g_hflip(struct v4l2_subdev *sd, s32 *value)
{
  struct mt9m114_info *info = to_state(sd);

  *value = info->flag_hflip;
  return 0;
}

static int mt9m114_s_hflip(struct v4l2_subdev *sd, int value)
{
  struct mt9m114_info *info = to_state(sd);
  
  info->flag_hflip = value;
  mt9m114_change_config(sd);
  return 0;
}


static int mt9m114_g_vflip(struct v4l2_subdev *sd, s32 *value)
{
  struct mt9m114_info *info = to_state(sd);

  *value = info->flag_vflip;
  return 0;
}

static int mt9m114_s_vflip(struct v4l2_subdev *sd, int value)
{
  struct mt9m114_info *info = to_state(sd);

  info->flag_vflip = value;
  mt9m114_change_config(sd);
  return 0;
}


static int mt9m114_s_sharpness(struct v4l2_subdev *sd, int value)
{
  int ret = 0;
  //if this uvc register is out of sync
  if(mt9m114_uvc_register_is_out_of_sync(sd, UVC_SHARPNESS))
  {//sync it
    dprintk(0,"MT9M114","MT9M114 :REG_UVC_SHARPNESS out of sync. syncing...\n"); 
    ret = mt9m114_sync_and_set_uvc_register_s32(sd, REG_UVC_SHARPNESS, 2, value);
    mt9m114_set_uvc_register_synced(sd, UVC_SHARPNESS);
  }
  else
  {//if the register is not out of sync: just set the value
    ret = mt9m114_write(sd, REG_UVC_SHARPNESS, 2, value); 
  }
  
  if(0 != ret)
  {
    dprintk(0,"MT9M114","MT9M114 : %s error writing value. errno: 0x%x\n", __func__, ret);
    return ret;
  }
  
  ret = check_uvc_status(sd, __func__);
  if(0 != ret)
  { //error has already been printed by check_uvc_status.
    return ret;
  }
  
  mt9m114_refresh(sd);
  
  return ret;
}

static int mt9m114_g_sharpness(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_UVC_SHARPNESS, 2, &v);

  *value = v;
  return ret;
}


static int mt9m114_s_auto_white_balance(struct v4l2_subdev *sd, int value)
{
  int ret = 0;
  dprintk(0,"MT9M114","MT9M114 : mt9m114_s_auto_white_balance(value=%i)\n", value);
  if(value==0x01)
  {
    //enable awb, disable auto exposure in between awb runs (this is the default)
    ret = mt9m114_write(sd, REG_AWB_AWB_MODE, 1, 0x02);
  }
  else
  {
    ret = mt9m114_write(sd, REG_AWB_AWB_MODE, 1, 0x00);
  }
  
  mt9m114_refresh(sd);

  return ret;
}

static int mt9m114_g_auto_white_balance(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_UVC_AUTO_WHITE_BALANCE_TEMPERATURE, 1, &v);

  *value = v;
  return ret;
}


static int mt9m114_s_backlight_compensation(struct v4l2_subdev *sd, int value)
{
  int ret = mt9m114_write(sd, REG_UVC_BACKLIGHT_COMPENSATION, 2, value);
  mt9m114_change_config(sd);
  return ret;
}

static int mt9m114_g_backlight_compensation(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_UVC_BACKLIGHT_COMPENSATION, 2, &v);

  *value = v;
  return ret;
}


/**
 * @note This will overwrite the values of UVC_FRAME_INTERVAL_CONTROL, UVC_EXPOSURE_TIME_ABSOLUTE_CONTROL
 *       and UVC_GAIN_CONTROL
 * @param sd
 * @param value
 * @return 
 */
static int mt9m114_s_auto_exposure(struct v4l2_subdev *sd, int value)
{
  int ret = 0;
  if(value == 0x01)
  {
    ret = mt9m114_write(sd, REG_UVC_AE_MODE, 1, 0x02);
  }
  else
  {
    ret = mt9m114_write(sd, REG_UVC_AE_MODE, 1, 0x01);
    
  }

  mt9m114_refresh(sd);
  return ret;
}

static int mt9m114_g_auto_exposure(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_UVC_AE_MODE, 1, &v);

  if(v==0x02)
  {
    *value = 0x01;
  }
  else
  {
    *value = 0x00;
  }

  return ret;
}


static int mt9m114_s_gain(struct v4l2_subdev *sd, int value)
{
  int ret = 0;
  //if this uvc register is out of sync
  if(mt9m114_uvc_register_is_out_of_sync(sd, UVC_GAIN))
  {//sync it
    dprintk(0,"MT9M114","MT9M114 :REG_UVC_GAIN out of sync. syncing...\n"); 
    ret = mt9m114_sync_and_set_uvc_register_s32(sd, REG_UVC_GAIN, 2, value);
    mt9m114_set_uvc_register_synced(sd, UVC_GAIN);
  }
  else
  {//if the register is not out of sync: just set the value
    ret = mt9m114_write(sd, REG_UVC_GAIN, 2, value); 
  }
  
  if(0 != ret)
  {
    dprintk(0,"MT9M114","MT9M114 : %s error writing value. errno: 0x%x\n", __func__, ret);
    return ret;
  }
  
  ret = check_uvc_status(sd, __func__);
  if(0 != ret)
  { //error has already been printed by check_uvc_status.
    return ret;
  }
  
  mt9m114_refresh(sd);

  return ret;
}

static int mt9m114_g_gain(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_UVC_GAIN, 2, &v);

  *value = v;
  return ret;
}


static int mt9m114_s_exposure(struct v4l2_subdev *sd, u32 value)
{
  int ret = 0;
  u32 shiftedValue = value << 2;
  //if this uvc register is out of sync
  if(mt9m114_uvc_register_is_out_of_sync(sd, UVC_EXPOSURE_TIME))
  {//sync it
    dprintk(0,"MT9M114","MT9M114 :UVC_EXPOSURE_TIME out of sync. syncing...\n"); 
    ret = mt9m114_sync_and_set_uvc_register_u32(sd, REG_UVC_EXPOSURE_TIME, 4, shiftedValue);
    mt9m114_set_uvc_register_synced(sd, UVC_EXPOSURE_TIME);
  }
  else
  {//if the register is not out of sync: just set the value
    ret = mt9m114_write(sd, REG_UVC_EXPOSURE_TIME, 4, shiftedValue); 
  }
  
  if(0 != ret)
  {
    dprintk(0,"MT9M114","MT9M114 : %s error writing value. errno: 0x%x\n", __func__, ret);
    return ret;
  }
  
  ret = check_uvc_status(sd, __func__);
  if(0 != ret)
  { //error has already been printed by check_uvc_status.
    return ret;
  }
  
  mt9m114_refresh(sd);

  return ret;
}

static int mt9m114_g_exposure(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_UVC_EXPOSURE_TIME, 4, &v);

  *value = v>>2;

  dprintk(0,"MT9M114","MT9M114 : mt9m114_g_exposure %x\n",v);

  return ret;
}


static int mt9m114_s_white_balance(struct v4l2_subdev *sd, u32 value)
{
  int ret;

  ret = mt9m114_write(sd, REG_AWB_COL_TEMP, 2, value);
  
  //FIXME read UVC_RESULT_STATUS to see if the value was ok.
  
  mt9m114_refresh(sd);
  return ret;
}

static int mt9m114_g_white_balance(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_AWB_COL_TEMP, 2, &v);

  *value = v;
  return ret;
}


static int mt9m114_s_auto_exposure_algorithm(struct v4l2_subdev *sd, int value)
{ 
  int ret = 0;
  if(value >= 0x0 && value <= 0x3)
  {
    ret = mt9m114_write(sd, REG_AE_ALGORITHM+1, 1, value);
  }
  else
  {
    return -EINVAL;
  }
  mt9m114_refresh(sd);
  return ret;
}

static int mt9m114_g_auto_exposure_algorithm(struct v4l2_subdev *sd, s32 *value)
{
  u32 v = 0;
  int ret = mt9m114_read(sd, REG_AE_ALGORITHM+1, 1, &v);
  *value = v & 0x3;
  return ret;
}


static int mt9m114_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
  //FIXME not implemented
  dprintk(0,"MT9M114","MT9M114 : %s not implemented\n",__func__);
      /*  a->bounds.left          = 0;
        a->bounds.top           = 0;
        a->bounds.width         =
                mt9m114_resolutions[ARRAY_SIZE(mt9m114_resolutions)-1].width;
        a->bounds.height        =
                mt9m114_resolutions[ARRAY_SIZE(mt9m114_resolutions)-1].height;
        a->defrect              = a->bounds;
        a->type                 = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        a->pixelaspect.numerator        = 1;
        a->pixelaspect.denominator      = 1;*/

        return 0;
}

static int mt9m114_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
  //FIXME not implemented
  dprintk(0,"MT9M114","MT9M114 : %s not implemented\n",__func__);
     /*   a->c.left               = 0;
        a->c.top                = 0;
        a->c.width              =
                mt9m114_resolutions[ARRAY_SIZE(mt9m114_resolutions)-1].width;
        a->c.height             =
                mt9m114_resolutions[ARRAY_SIZE(mt9m114_resolutions)-1].height;
        a->type                 = V4L2_BUF_TYPE_VIDEO_CAPTURE;*/

        return 0;
}



static int mt9m114_g_fade_to_black(struct v4l2_subdev *sd, s32 *value)
{  
  u32 v = 0;
  
  int ret = mt9m114_read(sd, REG_CAM_LL_MODE, 2, &v);
  *value = v;
  return ret;
}

static int mt9m114_s_fade_to_black(struct v4l2_subdev *sd, int value)
{
  s32 currentValue = 0;
  
  int ret = mt9m114_g_fade_to_black(sd, &currentValue);
  if (ret == 0)
  {
    if(value != 0)
    {
      currentValue |= (1 << 3);
    }
    else
    {
      currentValue &= (0 << 3);
    }
    
    ret = mt9m114_write(sd, REG_CAM_LL_MODE, 2, currentValue);
  }
  else
  {
      dprintk(0,"MT9M114","MT9M114 : %s Failed to get value\n", __func__);
  }
  
  mt9m114_refresh(sd);
  return ret;
  
}


/**
 * Is called by an application to ask which control commands are supported.
 * 
 * @param sd
 * @param qc
 * @return Returns EINVAL if the specified control is not supported. Otherwise returns 0 and fills 
 *         qc.
 */
static int mt9m114_queryctrl(struct v4l2_subdev *sd,
    struct v4l2_queryctrl *qc)
{
  dprintk(0,"MT9M114","MT9M114 : %s id:%x\n",__func__, qc->id);

  /* Fill in min, max, step and default value for these controls. */
  switch (qc->id) {
  case V4L2_CID_BRIGHTNESS:
    return v4l2_ctrl_query_fill(qc, 0, 255, 1, 55);
  case V4L2_CID_CONTRAST:
    return v4l2_ctrl_query_fill(qc, 16, 64, 1, 32);
  case V4L2_CID_SATURATION:
    return v4l2_ctrl_query_fill(qc, 0, 255, 1, 128);
  case V4L2_CID_HUE:
    return v4l2_ctrl_query_fill(qc, -22, 22, 1, 0);
  case V4L2_CID_VFLIP:
  case V4L2_CID_HFLIP:
    return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
  case V4L2_CID_SHARPNESS:
    return v4l2_ctrl_query_fill(qc, -7, 7, 1, 0);
  case V4L2_CID_EXPOSURE_AUTO:
    return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
  case V4L2_CID_AUTO_WHITE_BALANCE:
    return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
  case V4L2_CID_GAIN:
    return v4l2_ctrl_query_fill(qc, 0, 255, 1, 32);
  case V4L2_CID_EXPOSURE:
    return v4l2_ctrl_query_fill(qc, 0, 512, 1, 0);
  case V4L2_CID_DO_WHITE_BALANCE:
    return v4l2_ctrl_query_fill(qc, 0x0A8C, 0x1964, 1, 0x1964);
  case V4L2_CID_BACKLIGHT_COMPENSATION:
    return v4l2_ctrl_query_fill(qc, 0, 4, 1, 1);
  case V4L2_MT9M114_FADE_TO_BLACK:
      qc->minimum = 0;
      qc->maximum = 1;
      qc->step = 1;
      qc->default_value = 1;
      qc->reserved[0] = qc->reserved[1] = 0;
      strcpy(qc->name, "Fade to Black");
      qc->type = V4L2_CTRL_TYPE_BOOLEAN;
      qc->flags = 0;
    return 0;  
  }
  
  return -EINVAL;
}

static int mt9m114_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
  printk(KERN_INFO "[%s:%u]\n",__FUNCTION__,__LINE__);

  switch (ctrl->id) {
  case V4L2_CID_BRIGHTNESS:
    return mt9m114_g_brightness(sd, &ctrl->value);
  case V4L2_CID_CONTRAST:
    return mt9m114_g_contrast(sd, &ctrl->value);
  case V4L2_CID_SATURATION:
    return mt9m114_g_sat(sd, &ctrl->value);
  case V4L2_CID_HUE:
    return mt9m114_g_hue(sd, &ctrl->value);
  case V4L2_CID_VFLIP:
    return mt9m114_g_vflip(sd, &ctrl->value);
  case V4L2_CID_HFLIP:
    return mt9m114_g_hflip(sd, &ctrl->value);
  case V4L2_CID_SHARPNESS:
    return mt9m114_g_sharpness(sd, &ctrl->value);
  case V4L2_CID_EXPOSURE_AUTO:
    return mt9m114_g_auto_exposure(sd, &ctrl->value);
  case V4L2_CID_AUTO_WHITE_BALANCE:
    return mt9m114_g_auto_white_balance(sd, &ctrl->value);
  case V4L2_CID_GAIN:
    return mt9m114_g_gain(sd, &ctrl->value);
  case V4L2_CID_EXPOSURE:
    return mt9m114_g_exposure(sd, &ctrl->value);
  case V4L2_CID_DO_WHITE_BALANCE:
    return mt9m114_g_white_balance(sd, &ctrl->value);
  case V4L2_CID_BACKLIGHT_COMPENSATION:
    return mt9m114_g_backlight_compensation(sd, &ctrl->value);
  case V4L2_MT9M114_FADE_TO_BLACK:
    return mt9m114_g_fade_to_black(sd, &ctrl->value);
  }
  return -EINVAL;
}

static int mt9m114_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
  printk(KERN_INFO "[%s:%u]\n",__FUNCTION__,__LINE__);

  switch (ctrl->id) {
  case V4L2_CID_BRIGHTNESS:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_BRIGHTNESS 0x%x\n", ctrl->value);
    return mt9m114_s_brightness(sd, ctrl->value);
  case V4L2_CID_CONTRAST:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_CONTRAST 0x%x\n", ctrl->value);
    return mt9m114_s_contrast(sd, ctrl->value);
  case V4L2_CID_SATURATION:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_SATURATION 0x%x\n", ctrl->value);
    return mt9m114_s_sat(sd, ctrl->value);
  case V4L2_CID_HUE:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_HUE 0x%x\n", ctrl->value);
    return mt9m114_s_hue(sd, ctrl->value);
  case V4L2_CID_VFLIP:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_VFLIP 0x%x\n", ctrl->value);
    return mt9m114_s_vflip(sd, ctrl->value);
  case V4L2_CID_HFLIP:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_HFLIP 0x%x\n", ctrl->value);
    return mt9m114_s_hflip(sd, ctrl->value);
  case V4L2_CID_SHARPNESS:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_SHARPNESS 0x%x\n", ctrl->value);
    return mt9m114_s_sharpness(sd, ctrl->value);
  case V4L2_CID_EXPOSURE_AUTO:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_EXPOSURE_AUTO 0x%x\n", ctrl->value);
    return mt9m114_s_auto_exposure(sd, ctrl->value);
  case V4L2_CID_AUTO_WHITE_BALANCE:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_AUTO_WHITE_BALANCE 0x%x\n", ctrl->value);
    return mt9m114_s_auto_white_balance(sd, ctrl->value);
  case V4L2_CID_GAIN:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_GAIN 0x%x\n", ctrl->value);
    return mt9m114_s_gain(sd, ctrl->value);
  case V4L2_CID_EXPOSURE:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_EXPOSURE 0x%x\n", ctrl->value);
    return mt9m114_s_exposure(sd, ctrl->value);
  case V4L2_CID_DO_WHITE_BALANCE:
    dprintk(0,"MT9M114","MT9M114 :set  id: V4L2_CID_DO_WHITE_BALANCE 0x%x\n", ctrl->value);
    return mt9m114_s_white_balance(sd, ctrl->value);
  case V4L2_CID_BACKLIGHT_COMPENSATION:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_CID_BACKLIGHT_COMPENSATION 0x%x\n", ctrl->value);
    return mt9m114_s_backlight_compensation(sd, ctrl->value);
  case V4L2_MT9M114_FADE_TO_BLACK:
    dprintk(0,"MT9M114","MT9M114 :set id: V4L2_MT9M114_FADE_TO_BLACK 0x%x\n", ctrl->value);
    return mt9m114_s_fade_to_black(sd, ctrl->value); 
  default:
    dprintk(0,"MT9M114","MT9M114 :set id: ERROR DEFAULT CASE0x%x\n", ctrl->value);
  }
  return -EINVAL;
}


static int mt9m114_g_chip_ident(struct v4l2_subdev *sd,
    struct v4l2_dbg_chip_ident *chip)
{
  struct i2c_client *client = v4l2_get_subdevdata(sd);

  printk(KERN_INFO "[%s:%u]\n",__FUNCTION__,__LINE__);

  return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_MT9M114, 0);
}



/* ----------------------------------------------------------------------- */

static const struct v4l2_subdev_core_ops mt9m114_core_ops = {
  .g_chip_ident = mt9m114_g_chip_ident,
  .g_ctrl = mt9m114_g_ctrl,
  .s_ctrl = mt9m114_s_ctrl,
  .queryctrl = mt9m114_queryctrl,
  .reset = mt9m114_reset,
  .init = mt9m114_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
  .g_register = mt9m114_g_register,
  .s_register = mt9m114_s_register,
#endif
};

static const struct v4l2_subdev_video_ops mt9m114_video_ops = {
  .enum_mbus_fmt = mt9m114_enum_fmt,
  .try_mbus_fmt = mt9m114_try_fmt,
  .s_mbus_fmt = mt9m114_s_fmt,
  .cropcap = mt9m114_cropcap,
  .g_crop = mt9m114_g_crop,
  .s_parm = mt9m114_s_parm,
  .g_parm = mt9m114_g_parm,
};

static const struct v4l2_subdev_ops mt9m114_subdev_ops = {
  .core = &mt9m114_core_ops,
  .video = &mt9m114_video_ops,
};

/* ----------------------------------------------------------------------- */

static int __devinit mt9m114_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  struct v4l2_subdev *sd;
  struct mt9m114_info *info;
  int ret;

  printk(KERN_INFO "[%s:%u] XXXXXXXXXXXXXXXXXXXXXXXXX\n",__FUNCTION__,__LINE__);
  printk(KERN_INFO "[%s:%u] XXXXXXXXXXXXXXXXXXXXXXXXX\n",__FUNCTION__,__LINE__);

  info = kzalloc(sizeof(struct mt9m114_info), GFP_KERNEL);
  if (info == NULL)
    return -ENOMEM;
  sd = &info->sd;
  v4l2_i2c_subdev_init(sd, client, &mt9m114_subdev_ops);

  /* Make sure it's an mt9m114 */
  ret = mt9m114_detect(sd);
  if (ret) {
    v4l_dbg(1, debug, client,
      "chip found @ 0x%x (%s) is not an mt9m114 chip.\n",
      client->addr , client->adapter->name);
    kfree(info);
    return ret;
  }
  v4l_info(client, "chip found @ 0x%02x (%s)\n",
      client->addr , client->adapter->name);

  info->fmt = &mt9m114_formats[0];
  info->flag_hflip = 0;
  info->flag_vflip = 0;

  return 0;
}


static __devexit int mt9m114_remove(struct i2c_client *client)
{
  struct v4l2_subdev *sd = i2c_get_clientdata(client);

  v4l2_device_unregister_subdev(sd);
  kfree(to_state(sd));
  return 0;
}

static const struct i2c_device_id mt9m114_id[] = {
  { DRIVER_NAME, 0 },
  { }
};

MODULE_DEVICE_TABLE(i2c, mt9m114_id);

static struct i2c_driver mt9m114_i2c_driver = {
   .driver = {
     .owner = THIS_MODULE,
     .name = DRIVER_NAME,
   },
  .probe = mt9m114_probe,
  .remove = mt9m114_remove,
  .id_table = mt9m114_id,
};

static int __init mt9m114_mod_init(void)
{
  int ret;
  printk(KERN_INFO "[%s:%u] XXXXXXXXXXXXXXXXXXXXXXXXX\n",__FUNCTION__,__LINE__);
  printk(KERN_INFO "[%s:%u] XXXXXXXXXXXXXXXXXXXXXXXXX\n",__FUNCTION__,__LINE__);

  ret = i2c_add_driver(&mt9m114_i2c_driver);
  if(ret != 0)
    printk("[MT9M114] I2C device init Faild! return(%d) \n",  ret);

  printk("[MT9M114] I2C device init Sucess\n");

  return ret;
}

static void __exit mt9m114_mod_exit(void)
{
  i2c_del_driver(&mt9m114_i2c_driver);
}

module_init(mt9m114_mod_init);
module_exit(mt9m114_mod_exit);

MODULE_DESCRIPTION("Micron/Aptina MT9M114 Camera driver");
MODULE_AUTHOR("Joseph Pinkasfeld <joseph.pinkasfeld@gmail.com>");
MODULE_LICENSE("GPL");



