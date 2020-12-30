/***************************************************************
 * @file       trdpProc.h
 * @brief      列车通信网络方仿真平台TRDP协议通信结构体定义
 * @author     王浩洋
 * @version    TRDP通信协议1.0
 * @date       2020-12 
 **************************************************************/
#ifndef TRDP_PROC_H
#define TRDP_PROC_H

//TRDP 测试 
typedef struct
{
	unsigned short year;                  //年
	unsigned char month;                  //月
    unsigned char day;                  //日
}PACKET_HEAD;

//EGWM 通过1100公共端口发送数据结构体 -----------------------------------------------
//Byte0~99 为 EGWM 公共端口数据
typedef struct
{
	unsigned char highByteOfLifeSign;     //生命信号高字节
	unsigned char lowByteOfLifeSign;      //生命信号低字节
	unsigned char highByteOfTrainNumber;  //列车号高字节
	unsigned char lowByteOfTrainNumber;   //列车号低字节
	unsigned char unixTimeHH;             //UNIX-Time HH
	unsigned char unixTimeHL;             //UNIX-Time HL
	unsigned char unixTimeLH;             //UNIX-Time LH
	unsigned char unixTimeLL;             //UNIX-Time LL
	unsigned char week;                   //week(1-7)  
    
    unsigned char reserverd1;     //保留字1，一个字节
    unsigned char unixTimeValid;          //UNIX 有效 

    unsigned char year;          //年
    unsigned char month;         //月
    unsigned char day;           //日
    unsigned char hour;          //时
    unsigned char minute;        //分
    unsigned char second;        //秒
    
    unsigned char reserverd2;       //保留字2，一个字节
    unsigned char timeValid;              //时间 有效 

    unsigned char highByteOfA1WheelDiameter;   //A1 车轮径高字节 
    unsigned char lowByteOfA1WheelDiameter;   //A1 车轮径低字节 
    unsigned char highByteOfB1WheelDiameter;   //B1 车轮径高字节 
    unsigned char lowByteOfB1WheelDiameter;   //B1 车轮径低字节 
    unsigned char highByteOfC1WheelDiameter;   //C1 车轮径高字节 
    unsigned char lowByteOfC1WheelDiameter;   //C1 车轮径低字节 
    unsigned char highByteOfC2WheelDiameter;   //C2 车轮径高字节 
    unsigned char lowByteOfC2WheelDiameter;   //C2 车轮径低字节 
    unsigned char highByteOfB2WheelDiameter;   //B2 车轮径高字节 
    unsigned char lowByteOfB2WheelDiameter;   //B2 车轮径低字节 
    unsigned char highByteOfA2WheelDiameter;   //A1 车轮径高字节 
    unsigned char lowByteOfA2WheelDiameter;   //A1 车轮径低字节 

    unsigned short reserverd3;     //保留字3，两个字节
    unsigned char wheelValid;   //轮径设置 有效 高7-2w位 分高字节别代表A2 B2 ... B1 A1 
    
    unsigned short reserverd4[33];     //保留字14，预留34-99位置的字节
}EGWM_PUBLIC_DATA;	 
//-----------------------------------------------------------------------------------------------------------//

//EGWM 通过1104公共端口发送给 EDCU数据 结构体 -----------------------------------------------
//Byte0~99 为 EGWM 发送给 EDCU 的数据
typedef struct
{
	unsigned char terminalActive;                 //占有标志 高7-6位分别代表 TC2、TC1
	unsigned char centralDoorSign;                //门信号 6 5 3 2 分别代表 右门关、开，左门关、开。0位代表限电模式
	unsigned char delayTimeHBeforeClosing;        //门关闭延时时间 H
	unsigned char delayTimeLBeforeClosing;        //门关闭延时时间 L
	unsigned char delayTimeHBeforeOpening;        //门打开延时时间 H
	unsigned char delayTimeLBeforeOpening;        //门打开延时时间 L
	unsigned char obstructionDetectionDelayTimeH; //障碍物检测延时时间 H
	unsigned char obstructionDetectionDelayTimeL; //障碍物检测延时时间 L
	unsigned char doorOpenTimeH;                  //开门过程持续时间 H 
    unsigned char doorOpenTimeL;                  //开门过程持续时间 L 
	unsigned char doorCloseTimeH;                 //关门过程持续时间 H 
	unsigned char doorCloseTimeL;                 //关门过程持续时间 L

	unsigned char closingAttemptsHAfterObstructionDetectionInClosingSequence;    //关闭过程中障碍物探测的关闭次数 H
	unsigned char closingAttemptsLAfterObstructionDetectionInClosingSequence;    //关闭过程中障碍物探测的关闭次数 L
	unsigned char openingAttemptsHAfterObstructionDetectionInOpeningSequence;    //开启过程中障碍物探测的开启次数 H                 //开门过程持续时间 H 
	unsigned char openingAttemptsLAfterObstructionDetectionInOpeningSequence;    //开启过程中障碍物探测的开启次数 L                 //开门过程持续时间 H 
	unsigned char reopenDistanceHAfterObstructionDetectionInClosingSequence;     //开启过程中障碍物探测的开启次数 H                 //开门过程持续时间 H 
	unsigned char reopenDistanceLAfterObstructionDetectionInClosingSequence;     //开启过程中障碍物探测的开启次数 L                //开门过程持续时间 H 

    unsigned char vcuLifeSignH;                  //VCU 生命信号 H
	unsigned char vcuLifeSignL;                  //VCU 生命信号 L 
	
    unsigned short reserverd1[40];     //保留字1，预留20-99位置的字节
gData
}EGWM_TO_EDCU;

typedef struct
{
	unsigned char terminalActive;                 //占有标志 高7-6位分别代表 TC2、TC1
	unsigned char centralDoorSign;                //门信号 6 5 3 2 分别代表 右门关、开，左门关、开。0位代表限电模式
}EGWM_TO_EDCU_SIGN1;

typedef struct
{
	unsigned char delayTimeHBeforeClosing;        //门关闭延时时间 H
	unsigned char delayTimeLBeforeClosing;        //门关闭延时时间 L
	unsigned char delayTimeHBeforeOpening;        //门打开延时时间 H
	unsigned char delayTimeLBeforeOpening;        //门打开延时时间 L
	unsigned char obstructionDetectionDelayTimeH; //障碍物检测延时时间 H
	unsigned char obstructionDetectionDelayTimeL; //障碍物检测延时时间 L
	unsigned char doorOpenTimeH;                  //开门过程持续时间 H 
    unsigned char doorOpenTimeL;                  //开门过程持续时间 L 
	unsigned char doorCloseTimeH;                 //关门过程持续时间 H 
	unsigned char doorCloseTimeL;                 //关门过程持续时间 L
}EGWM_TO_EDCU_DOORTIME;

typedef struct
{
	unsigned char closingAttemptsHAfterObstructionDetectionInClosingSequence;    //关闭过程中障碍物探测的关闭次数 H
	unsigned char closingAttemptsLAfterObstructionDetectionInClosingSequence;    //关闭过程中障碍物探测的关闭次数 L
	unsigned char openingAttemptsHAfterObstructionDetectionInOpeningSequence;    //开启过程中障碍物探测的开启次数 H    
	unsigned char openingAttemptsLAfterObstructionDetectionInOpeningSequence;    //开启过程中障碍物探测的开启次数 L    
	unsigned char reopenDistanceHAfterObstructionDetectionInClosingSequence;     //开启过程中障碍物探测的开启次数 H      
	unsigned char reopenDistanceLAfterObstructionDetectionInClosingSequence;     //开启过程中障碍物探测的开启次数 L  
}EGWM_TO_EDCU_DOORSEQUENCE;

typedef struct
{
    unsigned char vcuLifeSignH;                  //VCU 生命信号 H
	unsigned char vcuLifeSignL;                  //VCU 生命信号 L 
	
    unsigned short reserverd1[40];     //保留字1，预留20-99位置的字节
}EGWM_TO_EDCU_SIGN2;

//-----------------------------------------------------------------------------------------------------------//

//EDCU 通过1040-1047端口发送给 EGWM 数据结构体 -----------------------------------------------
//Byte0~99 为 EDCU 发送给 EGWM 的状态数据
typedef struct
{
	unsigned char obstructionAndDoor;             //状态标志，低位0表示门隔离，低位1表示检测障碍物
	unsigned char doorAndSign;                    //门信号标志
    unsigned char reserverd1[5];     //保留字1，预留2-6位置的字节

	unsigned char lifeSignMDCULifeSignH;        //MDCU 生命信号 H
	unsigned char lifeSignMDCULifeSignL;        //MDCU 生命信号 L
    unsigned char reserverd2;     //保留字2，预留9位置的字节
	unsigned char door1OpenAndCloseCounterHH;       //门打开延时时间 H
	unsigned char door1OpenAndCloseCounterHL;       //门打开延时时间 L
	unsigned char door1OpenAndCloseCounterLH;       //障碍物检测延时时间 H
	unsigned char door1OpenAndCloseCounterLL;       //障碍物检测延时时间 L
	unsigned char softwareVersionNumberHOfDoor1;                  //开门过程持续时间 H 
    unsigned char softwareVersionNumberMOfDoor1;                  //开门过程持续时间 L 
	unsigned char softwareVersionNumberLOfDoor1;                 //关门过程持续时间 H 
    unsigned char reserverd3[83];     //保留字3，预留17-99位置的字节
	
    //Byte100~199 为 EDCU 发送给 EGWM 的故障数据

    //Bit0 开门过程的电机电流监控在连续几次开门过程中启动
    //Bit1 门控器内部安全继电器故障
    //Bit2 门控器输出A1短路
    //Bit3 门控器输出A2短路
    //Bit4 门控器输出A3短路
    //Bit5 门控器输出A4短路
    //Bit6 用于启动电磁制动的门控器输出短路
    //Bit7 单门安全回路故障
    unsigned char errorsInDoorControl1;
    
    //Bit0 车门驱动电机电线破损
    //Bit1 限位开关“门已关闭和锁闭”故障
    //Bit2 反向锁闭机构故障
    //Bit3 门扇未经许可离开关闭和锁闭位置
    //Bit4 隔离装置故障
    //Bit5 车门未能在3秒内解锁
    //Bit6 门位置传感器故障
    //Bit7 关闭过程中障碍物探测在连续几次关闭过程中启动
    unsigned char errorsInDoorControl2;
    
    //Bit0 输出状态监控激活(输出A5 ,输入输入E10)
    //Bit1 门控器输出A6短路
    //Bit2 0
    //Bit3 0
    //Bit4 0
    //Bit5 0
    //Bit6 0
    //Bit7 0
    unsigned char errorsInDoorControl3;

    //Bit0 门控器输出A7短路
    //Bit1 门控器输出A8短路
    //Bit2 来自车辆控制单元的信号 不一致
    //Bit3 门控器维护按钮故障
    //Bit4 诊断记忆的备用电池故障
    //Bit5 VCU过来信号错误
    //Bit6 系统未在规定时间内探测到开门位置
    //Bit7 系统未在规定时间内探测到关闭位置
    unsigned char errorsInDoorControl4;
    
    unsigned  reserverd4[48];     //保留字4，预留104-199位置的字节

//结构体声明时不能初始化。
  // unsigned char reserverd4 = 0xA5;
    //unsigned char reserverd5 = 0x5A;
    unsigned char reserverd5;
    unsigned char reserverd6retstring;
unsigned char reserverd2;     //保留字2，预留9位置的字节erd9 = 0x5A;
    //unsigned char reserverd10 = 0xA5;
    //unsigned char reserverd11 = 0x5A;
    //unsigned char reserverd12 = 0xA5;
    //unsigned char reserverd13 = 0x5A;
    //unsigned char reserverd14 = 0xA5;
    //unsigned char reserverd15 = 0x5A;
    //unsigned char reserverd16 = 0xA5;
    //unsigned char reserverd17 = 0x5A;

    unsigned short checksum;  //校验

}EDCU_TO_EGWM;

//-----------------------------------------------------------------------------------------------------------//

//EGWM 通过1105端口发送给 HVAC数据 结构体 -----------------------------------------------
//Byte0~99 为 EGWM 发送给 HVAC 的数据
typedef struct
{
    //Byte0~3 为 EGWM 发送给 HVAC 的整车控制数据------------------------
    //Bit0 自动模式
    //Bit1 +1K
    //Bit2 -1K
    //Bit3 +2K
    //Bit4 -2K
    //Bit5 通风模式
    //Bit6 车内火灾模式
    //Bit7 车外火灾模式
    unsigned char modelState;
    //Bit0 列车载客1（1~100）
    //Bit1 列车载客1（101~200）
    //Bit2 列车载客1（>200）
    //Bit3 
    //Bit4 
    //Bit5 
    //Bit6 紧急通风模式
    //Bit7 预冷预热停止
    unsigned char trainPreheatingCooling;
    unsigned char reserverd1;     //保留字1，预留2位置的字节
    unsigned char lifeSign;       //生命信号
    //--------------------------------------------------------------//

    //Byte4~15 为 EGWM 发送给 HVAC 的单车控制数据------------------------
    
    //HVAC1
    //Bit0 空调开
    //Bit1 空调关
    //Bit2 允许压缩机启动
    //Bit3 减载指令
    //Bit4 
    //Bit5 
    //Bit6 
    //Bit7 
    unsigned char airConditioningSwitch1;  
	unsigned char reserverd2;     //保留字2，预留5位置的字节

    //HVAC2
    //Bit0 空调开
    //Bit1 空调关
    //Bit2 允许压缩机启动
    //Bit3 减载指令
    //Bit4 
    //Bit5 
    //Bit6 
    //Bit7 
    unsigned char airConditioningSwitch2;  
	unsigned char reserverd3;     //保留字3，预留7位置的字节

    //HVAC3
    //Bit0 空调开
    //Bit1 空调关
    //Bit2 允许压缩机启动
    //Bit3 减载指令
    //Bit4 
    //Bit5 
    //Bit6 
    //Bit7 
    unsigned char airConditioningSwitch3;  
	unsigned char reserverd4;     //保留字4，预留9位置的字节

    //HVAC4
    //Bit0 空调开
    //Bit1 空调关
    //Bit2 允许压缩机启动
    //Bit3 减载指令
    //Bit4 
    //Bit5 
    //Bit6 
    //Bit7 
    unsigned char airConditioningSwitch4;  
	unsigned char reserverd5;     //保留字5，预留11位置的字节

    //HVAC5
    //Bit0 空调开
    //Bit1 空调关
    //Bit2 允许压缩机启动
    //Bit3 减载指令
    //Bit4 
    //Bit5 
    //Bit6 
    //Bit7 
    unsigned char airConditioningSwitch5;  
	unsigned char reserverd6;     //保留字6，预留13位置的字节

    //HVAC6
    //Bit0 空调开
    //Bit1 空调关
    //Bit2 允许压缩机启动
    //Bit3 减载指令
    //Bit4 
    //Bit5 
    //Bit6 
    //Bit7 
    unsigned char airConditioningSwitch6;  
	unsigned char reserverd7;     //保留字7，预留15位置的字节
	
    //--------------------------------------------------------------//

    unsigned short reserverd8[42];     //保留字8，预留16-99位置的字节
gData
}EGWM_TO_HVAC;
//-----------------------------------------------------------------------------------------------------------//

//EDCU 通过 1050\2050\...\6050 端口发送给 EGWM 和 OCS 的端口 -----------------------------------------------
//Byte0~99 为 EDCU 发送给 EGWM 的状态数据
typedef struct
{
    //Byte0~49 为 HVAC 机组 1 发送给 EGWM 状态数据-----------------------------------
	unsigned char highByteOfSoftwareVersionSet1;             //本车空调机组 1 软件版本高字节
    unsigned char lowByteOfSoftwareVersionSet1;             //本车空调机组 1 软件版本高字节

    unsigned char reserverd1[2];     //保留字1，两个字节

    //Bit0 机组 1 自动运行模式激活
    //Bit1 当前机组 1 停机模式
    //Bit2 当前机组 1 通风模式
    //Bit3 当前机组 1 预冷模式
    //Bit4 当前机组 1 预暖模式
    //Bit5 当前机组 1 制冷模式
    //Bit6 当前机组 1 制暖模式 
    //Bit7 当前机组 1 减载模式 
    unsigned char modelState1Set1;
    //Bit0 当前机组 1 紧急通风模式
    //Bit1 机组 1 火灾模式激活
    //Bit2 机组 1 强制通风模式激活
    //Bit3 机组 1 压缩机 1 运行
    //Bit4 机组 1 压缩机 2 运行
    //Bit5 机组 1 通风机运行
    //Bit6 机组 1 冷凝风机 1 运行
    //Bit7 机组 1 四通阀 11 运行 
    unsigned char modelState2Set1;
    //Bit0 机组 1 四通阀 12 运行
    //Bit1 机组 1 目标温度 +1°C 
    //Bit2 机组 1 目标温度 +2°C 
    //Bit3 机组 1 目标温度 -1°C
    //Bit4 机组 1 目标温度 -2°C
    //Bit5 机组 1 冷凝风机 2 运行
    //Bit6 机组 1 除霜系统 1 运行
    //Bit7 机组 1 除霜系统 2 运行 
    unsigned char modelState3Set1;

    unsigned char trainConfigurationStatus1;  //机组 1 当前车厢配置状态
    unsigned char modeSwitchStatus1;          //机组 1 模式开关当前状态
    unsigned char highByteOfTempSetPoint1;    //Temp_set_point_1 机组 1 设定温度高字节
    unsigned char lowByteOfTempSetPoint1;     //Temp_set_point_1 机组 1 设定温度低字节
    unsigned char highByteOfTempRetureAir1;   //Temp_reture_air_1 机组 1 回风温度高字节
    unsigned char lowByteOfTempRetureAir1;    //Temp_reture_air_1 机组 1 回风温度低字节
    unsigned char highByteOfTempFreshAir1;    //Temp_fresh_air_1 机组 1 新风温度高字节
    unsigned char lowByteOfTempFreshAir1;     //Temp_fresh_air_1 机组 1 新风温度低字节
    unsigned char highByteOfTempSupplyAir1;   //Temp_supply_air_1 机组 1 送风温度高字节
    unsigned char lowByteOfTempSupplyAir1;    //Temp_supply_air_1 机组 1 送风温度低字节

    unsigned char reserverd2;                 //保留字2，一个字节，在17位置
    unsigned char lifeSignH1;                  //生命信号 H
	unsigned char lifeSignL1;                  //生命信号 L
    unsigned short reserverd3[15];            //保留字3，预留20-49位置的字节

    //--------------------------------------------------------------//


    //Byte50~99 为 HVAC 机组 2 发送给 EGWM 状态数据-----------------------------------
	unsigned char highByteOfSoftwareVersionSet2;             //本车空调机组 2 软件版本高字节
    unsigned char lowByteOfSoftwareVersionSet2;             //本车空调机组 2 软件版本高字节

    unsigned char reserverd4[2];     //保留字4，两个字节

    //Bit0 机组 2 自动运行模式激活
    //Bit1 当前机组 2 停机模式
    //Bit2 当前机组 2 通风模式
    //Bit3 当前机组 2 预冷模式
    //Bit4 当前机组 2 预暖模式
    //Bit5 当前机组 2 制冷模式
    //Bit6 当前机组 2 制暖模式 
    //Bit7 当前机组 2 减载模式 
    unsigned char modelState1Set2;
    //Bit0 当前机组 2 紧急通风模式
    //Bit1 机组 2 火灾模式激活
    //Bit2 机组 2 强制通风模式激活
    //Bit3 机组 2 压缩机 1 运行
    //Bit4 机组 2 压缩机 2 运行
    //Bit5 机组 2 通风机运行
    //Bit6 机组 2 冷凝风机 1 运行
    //Bit7 机组 2 四通阀 11 运行 
    unsigned char modelState2Set2;
    //Bit0 机组 2 四通阀 12 运行
    //Bit1 机组 2 目标温度 +1°C 
    //Bit2 机组 2 目标温度 +2°C 
    //Bit3 机组 2 目标温度 -1°C
    //Bit4 机组 2 目标温度 -2°C
    //Bit5 机组 2 冷凝风机 2 运行
    //Bit6 机组 2 除霜系统 1 运行
    //Bit7 机组 2 除霜系统 2 运行 
    unsigned char modelState3Set2;

    unsigned char trainConfigurationStatus2;  //机组 2 当前车厢配置状态
    unsigned char modeSwitchStatus2;          //机组 2 模式开关当前状态
    unsigned char highByteOfTempSetPoint2;    //Temp_set_point_1 机组 2 设定温度高字节
    unsigned char lowByteOfTempSetPoint2;     //Temp_set_point_1 机组 2 设定温度低字节
    unsigned char highByteOfTempRetureAir2;   //Temp_reture_air_1 机组 2 回风温度高字节
    unsigned char lowByteOfTempRetureAir2;    //Temp_reture_air_1 机组 2 回风温度低字节
    unsigned char highByteOfTempFreshAir2;    //Temp_fresh_air_1 机组 2 新风温度高字节
    unsigned char lowByteOfTempFreshAir2;     //Temp_fresh_air_1 机组 2 新风温度低字节
    unsigned char highByteOfTempSupplyAir2;   //Temp_supply_air_1 机组 2 送风温度高字节
    unsigned char lowByteOfTempSupplyAir2;    //Temp_supply_air_1 机组 2 送风温度低字节

    unsigned char reserverd5;                 //保留字5，一个字节，在67位置
    unsigned char lifeSignH2;                  //生命信号 H
	unsigned char lifeSignL2;                  //生命信号 L
    unsigned short reserverd6[15];            //保留字6，预留70-99位置的字节

    //--------------------------------------------------------------//

    //Byte100~149 为 HVAC 机组 1 发送给 EGWM 故障数据-----------------------------------

    //Bit0 机组 1 严重故障
    //Bit1 机组 1 中等故障 
    //Bit2 机组 1 轻微故障 
    //Bit3 机组 1 通风模式故障
    //Bit4 机组 1 制冷模式故障
    //Bit5 机组 1 制热模式故障
    //Bit6 机组 1 除霜模式故障
    //Bit7 
    unsigned char faultStatus1Set1;        //机组 1 故障状态 1

    //Bit0 机组 1 紧急通风故障
    //Bit1 机组 1 通风机 1 过载故障
    //Bit2 机组 1 通风机 2 过载故障
    //Bit3 机组 1 冷凝风机 1 过载故障
    //Bit4 机组 1 冷凝风机 2 过载故障
    //Bit5 机组 1 压缩机 1 过载故障
    //Bit6 机组 1 压缩机 2 过载故障
    //Bit7 机组 1 压缩机 1 排气温度保护
    unsigned char faultStatus2Set1;        //机组 1 故障状态 2

    //Bit0 机组 1 压缩机 2 排气温度保护
    //Bit1 机组 1 压缩机 1 高压压力保护 
    //Bit2 机组 1 压缩机 2 高压压力保护
    //Bit3 机组 1 压缩机 1 低压压力保护
    //Bit4 机组 1 压缩机 2 低压压力保护
    //Bit5 机组 1 回风阀 1 故障
    //Bit6 机组 1 回风阀 2 故障 
    //Bit7 机组 1 回风阀 1、2 均故障 
    unsigned char faultStatus3Set1;        //机组 1 故障状态 3

    //Bit0 机组 1 新风阀 1 故障
    //Bit1 机组 1 新风阀 2 故障 
    //Bit2 机组 1 新风阀全 1、2 均故障 
    //Bit3 机组 1 主回路 AC380V 供电故障
    //Bit4 机组 1 主回路供电 AC380V 空开跳开
    //Bit5 机组 1 新风传感器故障
    //Bit6 机组 1 送风传感器故障 
    //Bit7 机组 1 回风传感器故障 
    unsigned char faultStatus4Set1;        //机组 1 故障状态 4

    //Bit0 机组 1 送风传感器均故障
    //Bit1 机组 1 回风传感器均故障 
    //Bit2 机组冷凝风机 2 接触器故障 
    //Bit3 机组压缩机 1 接触器故障
    //Bit4 机组压缩机 2 接触器故障 
    //Bit5 机组通风机接触器故障 
    //Bit6 机组冷凝风机 1 接触器故障 
    //Bit7 机组冷凝风机 2 接触器故障 
    unsigned char faultStatus5Set1;        //机组 1 故障状态 5

    //Bit0 机组 1 紧急通风接触器故障
    //Bit1
    //Bit2
    //Bit3
    //Bit4
    //Bit5
    //Bit6
    //Bit7
    unsigned char faultStatus6Set1;        //机组 1 故障状态 6
    unsigned short reserverd7[22];         //保留字7，预留106-149位置的字节
    //--------------------------------------------------------------//

    //Byte150~199 为 HVAC 机组 2 发送给 EGWM 故障数据-----------------------------------

    //Bit0 机组 2 严重故障
    //Bit1 机组 2 中等故障 
    //Bit2 机组 2 轻微故障 
    //Bit3 机组 2 通风模式故障
    //Bit4 机组 2 制冷模式故障
    //Bit5 机组 2 制热模式故障
    //Bit6 机组 2 除霜模式故障
    //Bit7 
    unsigned char faultStatus1Set2;        //机组 2 故障状态 1

    //Bit0 机组 2 紧急通风故障
    //Bit1 机组 2 通风机 1 过载故障
    //Bit2 机组 2 通风机 2 过载故障
    //Bit3 机组 2 冷凝风机 1 过载故障
    //Bit4 机组 2 冷凝风机 2 过载故障
    //Bit5 机组 2 压缩机 1 过载故障
    //Bit6 机组 2 压缩机 2 过载故障
    //Bit7 机组 2 压缩机 1 排气温度保护
    unsigned char faultStatus2Set2;        //机组 2 故障状态 2

    //Bit0 机组 2 压缩机 2 排气温度保护
    //Bit1 机组 2 压缩机 1 高压压力保护 
    //Bit2 机组 2 压缩机 2 高压压力保护
    //Bit3 机组 2 压缩机 1 低压压力保护
    //Bit4 机组 2 压缩机 2 低压压力保护
    //Bit5 机组 2 回风阀 1 故障
    //Bit6 机组 2 回风阀 2 故障 
    //Bit7 机组 2 回风阀 1、2 均故障 
    unsigned char faultStatus3Set2;        //机组 2 故障状态 3

    //Bit0 机组 2 新风阀 1 故障
    //Bit1 机组 2 新风阀 2 故障 
    //Bit2 机组 2 新风阀全 1、2 均故障 
    //Bit3 机组 2 主回路 AC380V 供电故障
    //Bit4 机组 2 主回路供电 AC380V 空开跳开
    //Bit5 机组 2 新风传感器故障
    //Bit6 机组 2 送风传感器故障 
    //Bit7 机组 2 回风传感器故障 
    unsigned char faultStatus4Set2;        //机组 1 故障状态 4

    //Bit0 机组 2 送风传感器均故障
    //Bit1 机组 2 回风传感器均故障 
    //Bit2 机组冷凝风机 2 接触器故障 
    //Bit3 机组压缩机 1 接触器故障
    //Bit4 机组压缩机 2 接触器故障 
    //Bit5 机组通风机接触器故障 
    //Bit6 机组冷凝风机 1 接触器故障 
    //Bit7 机组冷凝风机 2 接触器故障 
    unsigned char faultStatus5Set2;        //机组 2 故障状态 5

    //Bit0 机组 1 紧急通风接触器故障
    //Bit1
    //Bit2
    //Bit3
    //Bit4
    //Bit5
    //Bit6
    //Bit7
    unsigned char faultStatus6Set2;        //机组 2 故障状态 6
    unsigned short reserverd8[22];         //保留字7，预留156-199位置的字节
    //--------------------------------------------------------------//


    //Byte200~299 为 HVAC 控制器发送给 OCS 的记录数据格式-----------------------------------

    //--------------------------------------------------------------//

}HVAC_TO_EGWM;

//-----------------------------------------------------------------------------------------------------------//






#endif