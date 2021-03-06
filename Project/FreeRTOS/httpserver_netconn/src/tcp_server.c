/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#include "stdlib.h"
#include "string.h"
#include "stm32f4xx.h"
#include "tcp_server.h"
#include "crc_16.h"
#include "flash.h"
#include "rtc.h"
#include "FreeRTOSConfig.h"
#include "cjson.h"

#include "lwip/opt.h"

#if LWIP_NETCONN

#include "lwip/sys.h"
#include "lwip/api.h"

#define TCPCLIENT_THREAD_PRIO   ( ( configMAX_PRIORITIES - 5 ) ) /*需要注意任务优先级的分配，与各任务之间的调用*/


/*sign in union*/
__attribute__ ((aligned (1)))
//union _SignIn_Union SignIn_Union;

/*TCP client send data mark  */    
uint8_t tcp_client_flag;

/*交易流水号（累加）*/
uint32_t BatchNum = 0;

/*TAG table Value detail*/
uint8_t  Tid[TidLen] = {0x10,0x00,0x00,0x38};
uint8_t  NomiNum[NomiNumLen]={0x10,0x00,0x01};
uint8_t  Bno[BnoLen]={0x00,0x00,0x00};
uint8_t  Brw[BrwLen]={0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t  DealData[DealDataLen]={0x00,0x00,0x00,0x00};
uint8_t  DealTime[DealTimeLen]={0x00,0x00,0x00};
uint8_t  DevArae[DevAraeLen]={0x17,0x03,0x02};
uint8_t  DevSite[DevSiteLen]={0x17,0x03,0x02,0x07};
uint8_t  AppVer[AppVerLen]={"010312"};  //V1.3.12,则为字符串“010312”
uint8_t  ParaFileVer[ParaFileVerLen]={"010313"};  //V1.3.12,则为字符串“010312”
uint8_t  BusiDatFileVer[BusiDatFileVerLen]={"010314"};  //V1.3.12,则为字符串“010312”
uint8_t  CkSta[CkStaLen]={0x00,0x00};	 //自检状态
uint8_t  UpDatFlag[UpDatFlagLen]={0x00};  //更新标识
uint8_t  CoinNum[CoinNumLen]={0x00,0x10};	//硬币数
uint8_t  DevStatu[DevStatuLen]={0xE8,0x00};  //设备状态
uint8_t  Mac[MacLen]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; //加密密钥
//uint8_t  MealCompar[36*4]={0x00};
uint8_t  Tradvol[TradvolLen]={0x00,0x00,0x00,0x00,0x15,0x00};	//交易金额
uint8_t  MealId[MealIdLen]={0x10,0x00,0x01,0x38}; //餐品ID
uint8_t  MealQty[MealQtyLen]={0x01}; //餐品数量
uint8_t  MealName[MealNameLen]={0xB7,0xC6,0xC4,0xAA,0xCB,0xB9,0xC5,0xA3,0xC8,0xE2};	//餐品名
uint8_t  MealPrice[MealPriceLen]={0x00,0x00,0x00,0x00,0x15,0x00};		//餐品单价
uint8_t  PayType[PayTypeLen]={0x31};	//支付方式
uint8_t  Change[ChangeLen]={0x00,0x00,0x00,0x00,0x15,0x00};		//找零
uint8_t  RmnMealQty[RmnMealQtyLen]={0x00,0x1};	//剩余数量
uint8_t  TkMealFlag[TkMealFlagLen]={0x01};	//取餐标记 0x01:成功，0x02:失败
uint8_t  PosDevNum[PosDevNumLen]={"1111111111"};	//刷卡器设备号
uint8_t  PosTenantNum[PosTenantNumLen]={"222222"};	//刷卡器商户号
uint8_t  PosBatchNum[PosBatchNumLen]={"3333333333"};	//刷卡器流水号
uint8_t  PosUserNum[PosUserNumLen]={"444444444444444444444"};	//用户卡密码

static uint8_t decode_signin_data(uint8_t *p);
static uint8_t decode_mealcomp_data(uint8_t *p);
static uint8_t decode_statupload_data(uint8_t *p);
static uint8_t decode_echo_data(uint8_t *p);
static uint8_t decode_getmeal_data(uint8_t *p);

/*******************************************************************************
 * 函数名称:Time_Conver  转换时间数据                                                            
 * 描    述:void                                                                   
 *                                                                               
 * 输    入:无                                                                     
 * 输    出:无                                                                     
 * 返    回:无                                                                  
 * 修改日期:2015年5月22日                                                                    
 *******************************************************************************/
void Time_Conver(DATA_TIME_STRUCT *pDataTime)
{
	DealData[0] = 0x20;
	DealData[1] = pDataTime->Year;
	DealData[2] = pDataTime->Month;
	DealData[3] = pDataTime->Date;
	DealTime[0] = pDataTime->Hours;
	DealTime[1] = pDataTime->Minutes;
	DealTime[2] = pDataTime->Senconds;
}

/*******************************************************************************
 * 函数名称:BatchNum_Conver  转换流水号                                                            
 * 描    述:void                                                                   
 *                                                                               
 * 输    入:无                                                                     
 * 输    出:无                                                                     
 * 返    回:无                                                                  
 * 修改日期:2015年5月22日                                                                    
 *******************************************************************************/
void BatchNum_Conver(DATA_TIME_STRUCT *pDataTime,uint8_t * batch_num)
{
	batch_num[0] = 0x20;
	batch_num[1] = pDataTime->Year;
	batch_num[2] = pDataTime->Month;
	batch_num[3] = pDataTime->Date;
	batch_num[4] = pDataTime->Hours;
	batch_num[5] = pDataTime->Minutes;
	batch_num[6] = pDataTime->Senconds;

#if 0
	batch_num[0] = 0;
	batch_num[1] = 0;
	batch_num[2] = 0;
	batch_num[3] =  (uint8_t) ((BatchNum >> 24) & 0xFF);
	batch_num[4] =  (uint8_t) ((BatchNum >> 16) & 0xFF);
	batch_num[5] =  (uint8_t) ((BatchNum >> 8) & 0xFF);  
	batch_num[6] =  (uint8_t) (BatchNum & 0xFF);	
	++BatchNum; //流水号增加
#endif
}
/**
  * @brief  获取签到结构提的数据
  * @param  NewState: new state of the Prefetch Buffer.
  *          This parameter  can be: ENABLE or DISABLE.
  * @retval None
  */
uint16_t get_signin_data(uint8_t *p)
{
	//union _SignIn_Union SignIn_Union;
	SignIn_Union   *pSignIn_Union;
	pSignIn_Union = (SignIn_Union *)pvPortMalloc(sizeof(SignIn_Union));
	/*--GetSignInPacketBuff--TAG value and length*/
	pSignIn_Union->SignIn.Tid = TidChl;
	pSignIn_Union->SignIn.Tid_Len[0] = (TidLen&0xFF00)>>4;
	pSignIn_Union->SignIn.Tid_Len[1] =  TidLen&0x00FF;
	pSignIn_Union->SignIn.NomiNum = NomiNumChl;
	pSignIn_Union->SignIn.NomiNum_Len[0] = (NomiNumLen&0xFF00)>>4;
	pSignIn_Union->SignIn.NomiNum_Len[1] = NomiNumLen&0x00FF;
	pSignIn_Union->SignIn.Brw = BrwChl;
	pSignIn_Union->SignIn.Brw_Len[0] = (BrwLen&0xFF00)>>4;;
	pSignIn_Union->SignIn.Brw_Len[1] = BrwLen&0x00FF;
	pSignIn_Union->SignIn.Bno = BnoChl;
	pSignIn_Union->SignIn.Bno_Len[0] = (BnoLen&0xFF00)>>4;
	pSignIn_Union->SignIn.Bno_Len[1] = BnoLen&0x00FF;
	pSignIn_Union->SignIn.DevArae = DevAraeChl;
	pSignIn_Union->SignIn.DevArae_Len[0] = (DevAraeLen&0xFF00)>>4;
	pSignIn_Union->SignIn.DevArae_Len[1] = DevAraeLen&0x00FF;
	pSignIn_Union->SignIn.DevSite = DevSiteChl;
	pSignIn_Union->SignIn.DevSite_Len[0] = (DevSiteLen&0xFF00)>>4;
	pSignIn_Union->SignIn.DevSite_Len[1] = DevSiteLen&0x00FF;
	pSignIn_Union->SignIn.AppVer = AppVerChl;
	pSignIn_Union->SignIn.AppVer_Len[0] = (AppVerLen&0xFF00)>>4;
	pSignIn_Union->SignIn.AppVer_Len[1] = AppVerLen&0x00FF;
	pSignIn_Union->SignIn.ParaFileVer = ParaFileVerChl;
	pSignIn_Union->SignIn.ParaFileVer_Len[0] = (ParaFileVerLen&0xFF00)>>4;
	pSignIn_Union->SignIn.ParaFileVer_Len[1] = ParaFileVerLen&0x00FF;
	pSignIn_Union->SignIn.BusiDatFileVer = BusiDatFileVerChl;
	pSignIn_Union->SignIn.BusiDatFileVer_Len[0] = (BusiDatFileVerLen&0xFF00)>>4;
	pSignIn_Union->SignIn.BusiDatFileVer_Len[1] = BusiDatFileVerLen&0x00FF;
	pSignIn_Union->SignIn.CkSta = CkStaChl;
	pSignIn_Union->SignIn.CkSta_Len[0] = (CkStaLen&0xFF00)>>4;; 
	pSignIn_Union->SignIn.CkSta_Len[1] = CkStaLen&0x00FF;  
	/*Get the TAG detail*/
	memcpy(pSignIn_Union->SignIn.Tid_Chl,Tid,sizeof(Tid));
	memcpy(pSignIn_Union->SignIn.NomiNum_Chl,NomiNum,sizeof(NomiNum));
	memcpy(pSignIn_Union->SignIn.Bno_Chl,Bno,sizeof(Bno));
	memcpy(pSignIn_Union->SignIn.Brw_Chl,Brw,sizeof(Brw));
	memcpy(pSignIn_Union->SignIn.DevArae_Chl,DevArae,sizeof(DevArae));
	memcpy(pSignIn_Union->SignIn.DevSite_Chl,DevSite,sizeof(DevSite));
	memcpy(pSignIn_Union->SignIn.AppVer_Chl,AppVer,sizeof(AppVer));
	memcpy(pSignIn_Union->SignIn.ParaFileVer_Chl,ParaFileVer,sizeof(ParaFileVer));
	memcpy(pSignIn_Union->SignIn.BusiDatFileVer_Chl,BusiDatFileVer,sizeof(BusiDatFileVer));
	memcpy(pSignIn_Union->SignIn.CkSta_Chl,CkSta,sizeof(CkSta));
	
	memcpy(p,pSignIn_Union,sizeof(SignIn_Union));
	
	vPortFree(pSignIn_Union);
	return  sizeof(SignIn_Union);
}

/**
  * @brief  获取餐品对比结构体数据
  * @param  NewState: new state of the Prefetch Buffer.
  *          This parameter  can be: ENABLE or DISABLE.
  * @retval None
  */
MealDetail_Union  mealdetail_union;
uint16_t get_mealcompar_data(uint8_t *p)
{
	MealCompar_Union *pMealCompar_Union;
	//MealCompar_Union  sMealCompar_Union;
	pMealCompar_Union = (MealCompar_Union *)pvPortMalloc(sizeof(MealCompar_Union));
	/*--GetSignInPacketBuff--TAG value and length*/
	pMealCompar_Union->MealCompar.Tid = TidChl;
	//sMealCompar_Union. MealCompar.Tid = TidChl;
	pMealCompar_Union->MealCompar.Tid_Len[0] = (TidLen&0xFF00)>>4;
	//sMealCompar_Union. MealCompar.Tid_Len[0] = (TidLen&0xFF00)>>4;
	pMealCompar_Union->MealCompar.Tid_Len[1] =  TidLen&0x00FF;
	//sMealCompar_Union. MealCompar.Tid_Len[1] =  TidLen&0x00FF;
	pMealCompar_Union->MealCompar.Bno = BnoChl;
	//sMealCompar_Union.MealCompar.Bno = BnoChl;
	pMealCompar_Union->MealCompar.Bno_Len[0] = (BnoLen&0xFF00)>>4;
	//sMealCompar_Union. MealCompar.Bno_Len[0] = (BnoLen&0xFF00)>>4;
	pMealCompar_Union->MealCompar.Bno_Len[1] =  BnoLen&0x00FF;
	//sMealCompar_Union. MealCompar.Bno_Len[1] =  BnoLen&0x00FF;
	pMealCompar_Union->MealCompar.Brw = BrwChl;
	//sMealCompar_Union. MealCompar.Brw = BrwChl;
	pMealCompar_Union->MealCompar.Brw_Len[0] = (BrwLen&0xFF00)>>4;
	//sMealCompar_Union. MealCompar.Brw_Len[0] = (BrwLen&0xFF00)>>4;
	pMealCompar_Union->MealCompar.Brw_Len[1] =  BrwLen&0x00FF;
	//sMealCompar_Union. MealCompar.Brw_Len[1] =  BrwLen&0x00FF;
	pMealCompar_Union->MealCompar.DevArae = DevAraeChl;
	//sMealCompar_Union. MealCompar.DevArae = DevAraeChl;
	pMealCompar_Union->MealCompar.DevArae_Len[0] = (DevAraeLen&0xFF00)>>4;
	//sMealCompar_Union. MealCompar.DevArae_Len[0] = (DevAraeLen&0xFF00)>>4;
	pMealCompar_Union->MealCompar.DevArae_Len[1] =  DevAraeLen&0x00FF;
	//sMealCompar_Union. MealCompar.DevArae_Len[1] =  DevAraeLen&0x00FF;
	pMealCompar_Union->MealCompar.DevSite = DevSiteChl;
	//sMealCompar_Union. MealCompar.DevSite = DevSiteChl;
	pMealCompar_Union->MealCompar.DevSite_Len[0] = (DevSiteLen&0xFF00)>>4;
	//sMealCompar_Union. MealCompar.DevSite_Len[0] = (DevSiteLen&0xFF00)>>4;
	pMealCompar_Union->MealCompar.DevSite_Len[1] =  DevSiteLen&0x00FF;
	//sMealCompar_Union. MealCompar.DevSite_Len[1] =  DevSiteLen&0x00FF;
	pMealCompar_Union->MealCompar.MealCompar = MealComparChl;
	//sMealCompar_Union. MealCompar.MealCompar = MealComparChl;
	pMealCompar_Union->MealCompar.MealCompar_Len[0] = (MealComparLen&0xFF00)>>4;
	//sMealCompar_Union. MealCompar.MealCompar_Len[0] = (MealComparLen&0xFF00)>>4;
	pMealCompar_Union->MealCompar.MealCompar_Len[1] =  MealComparLen&0x00FF;
	//sMealCompar_Union. MealCompar.MealCompar_Len[1] =  MealComparLen&0x00FF;
	pMealCompar_Union->MealCompar.Mac = MacChl;
	//sMealCompar_Union. MealCompar.Mac = MacChl;
	pMealCompar_Union->MealCompar.Mac_Len[0] = (MacLen&0xFF00)>>4;
	//sMealCompar_Union. MealCompar.Mac_Len[0] = (MacLen&0xFF00)>>4;
	pMealCompar_Union->MealCompar.Mac_Len[1] =  MacLen&0x00FF;
	//sMealCompar_Union. MealCompar.Mac_Len[1] =  MacLen&0x00FF;
	
	memcpy(pMealCompar_Union->MealCompar.Tid_Chl,Tid,sizeof(Tid));
	//memcpy(sMealCompar_Union. MealCompar.Tid_Chl,Tid,sizeof(Tid));
	memcpy(pMealCompar_Union->MealCompar.Bno_Chl,Bno,sizeof(Bno));
	//memcpy(sMealCompar_Union. MealCompar.Bno_Chl,Bno,sizeof(Bno));
	memcpy(pMealCompar_Union->MealCompar.Brw_Chl,Brw,sizeof(Brw));
	//memcpy(sMealCompar_Union. MealCompar.Brw_Chl,Brw,sizeof(Brw));
	memcpy(pMealCompar_Union->MealCompar.DevArae_Chl,DevArae,sizeof(DevArae));
	//memcpy(sMealCompar_Union. MealCompar.DevArae_Chl,DevArae,sizeof(DevArae));
	memcpy(pMealCompar_Union->MealCompar.DevSite_Chl,DevSite,sizeof(DevSite));
	//memcpy(sMealCompar_Union. MealCompar.DevSite_Chl,DevSite,sizeof(DevSite));
	/*start------获取餐品对比的相关信息，拷贝到相关结构体中--------------------*/
	for (int i = 0; i < MealKindTotoal; ++i)
	{
		memcpy(mealdetail_union.MealDetail[i].MealID,Meal_Union.Meal[i].MealID,sizeof(Meal_Union.Meal[i].MealID));
		memcpy(mealdetail_union.MealDetail[i].MealName,Meal_Union.Meal[i].MealName,sizeof(Meal_Union.Meal[i].MealName));
		memcpy(mealdetail_union.MealDetail[i].MealCnt,Meal_Union.Meal[i].MealCnt,sizeof(Meal_Union.Meal[i].MealCnt));
		memcpy(mealdetail_union.MealDetail[i].MealPrice,Meal_Union.Meal[i].MealPrice,sizeof(Meal_Union.Meal[i].MealPrice));
		memcpy(mealdetail_union.MealDetail[i].MealType,Meal_Union.Meal[i].MealType,sizeof(Meal_Union.Meal[i].MealType));
	}
	memcpy(pMealCompar_Union->MealCompar.MealCompar_Chl,mealdetail_union.MealDetailBuf,sizeof(mealdetail_union.MealDetailBuf));
	//memcpy(sMealCompar_Union.MealCompar.MealCompar_Chl,mealdetail_union.MealDetailBuf,sizeof(mealdetail_union.MealDetailBuf));
	/*------------获取餐品对比的相关信息，拷贝到相关结构体中-----------------end*/
	memcpy(pMealCompar_Union->MealCompar.Mac_Chl,Mac,sizeof(Mac));
	//memcpy(sMealCompar_Union. MealCompar.Mac_Chl,Mac,sizeof(Mac));
	memcpy(p,pMealCompar_Union->MealComparBuf,sizeof(pMealCompar_Union->MealComparBuf));
	//memcpy(p,sMealCompar_Union.MealComparBuf,sizeof(sMealCompar_Union.MealComparBuf));
	vPortFree(pMealCompar_Union);
	return  sizeof(pMealCompar_Union->MealComparBuf);  
}
  
/**
  * @brief  获取状态上送结构体数据
  * @param  NewState: new state of the Prefetch Buffer.
  *          This parameter  can be: ENABLE or DISABLE.
  * @retval None
  */
uint16_t get_statupload_data(uint8_t *p)
{
	StatuUpload_Union *pStatuUpload_Union;
	pStatuUpload_Union = (StatuUpload_Union *)pvPortMalloc(sizeof(pStatuUpload_Union));
	/*--GetStatuUploadPacketBuff--TAG value and length*/
	pStatuUpload_Union->StatuUpload.Tid = TidChl;
	pStatuUpload_Union->StatuUpload.Tid_Len[0] = (TidLen&0xFF00)>>4;
	pStatuUpload_Union->StatuUpload.Tid_Len[1] =  TidLen&0x00FF;
	pStatuUpload_Union->StatuUpload.Brw = BrwChl;
	pStatuUpload_Union->StatuUpload.Brw_Len[0] = (BrwLen&0xFF00)>>4;;
	pStatuUpload_Union->StatuUpload.Brw_Len[1] = BrwLen&0x00FF;
	pStatuUpload_Union->StatuUpload.Bno = BnoChl;
	pStatuUpload_Union->StatuUpload.Bno_Len[0] = (BnoLen&0xFF00)>>4;
	pStatuUpload_Union->StatuUpload.Bno_Len[1] = BnoLen&0x00FF;  
	pStatuUpload_Union->StatuUpload.CoinNum = CoinNumChl;
	pStatuUpload_Union->StatuUpload.CoinNum_Len[0] = (CoinNumLen&0xFF00)>>4;
	pStatuUpload_Union->StatuUpload.CoinNum_Len[1] = CoinNumLen&0x00FF;  
	pStatuUpload_Union->StatuUpload.DevStatu = DevStatuChl;
	pStatuUpload_Union->StatuUpload.DevStatu_Len[0] = (DevStatuLen&0xFF00)>>4;
	pStatuUpload_Union->StatuUpload.DevStatu_Len[1] = DevStatuLen&0x00FF; 
	
	memcpy(pStatuUpload_Union->StatuUpload.Tid_Chl,Tid,sizeof(Tid));
	memcpy(pStatuUpload_Union->StatuUpload.Brw_Chl,Brw,sizeof(Brw));
	memcpy(pStatuUpload_Union->StatuUpload.Bno_Chl,Bno,sizeof(Bno));
	memcpy(pStatuUpload_Union->StatuUpload.CoinNum_Chl,CoinNum,sizeof(CoinNum));
	memcpy(pStatuUpload_Union->StatuUpload.DevStatu_Chl,Tid,sizeof(DevStatu));

	memcpy(p,pStatuUpload_Union,sizeof(pStatuUpload_Union->StatuUploafBuf));
	vPortFree(pStatuUpload_Union);
	return  sizeof(pStatuUpload_Union->StatuUploafBuf);
}

/**
  * @brief  获取回响测试结构体数据
  * @param  NewState: new state of the Prefetch Buffer.
  *          This parameter  can be: ENABLE or DISABLE.
  * @retval None
  */
uint16_t get_echo_data(uint8_t *p)
{
	Echo_Union *pEcho_Union;
	pEcho_Union = (Echo_Union *)pvPortMalloc(sizeof(pEcho_Union));
	
	pEcho_Union->Echo.Tid = TidChl;
	pEcho_Union->Echo.Tid_Len[0] = (TidLen&0xFF00)>>4;
	pEcho_Union->Echo.Tid_Len[1] =  TidLen&0x00FF;
	pEcho_Union->Echo.DealData = DealDataChl;
	pEcho_Union->Echo.DealData_Len[0] = (DealDataLen&0xFF00)>>4;
	pEcho_Union->Echo.DealData_Len[1] =  DealDataLen&0x00FF;	
	pEcho_Union->Echo.DealTime = DealTimeChl;
	pEcho_Union->Echo.DealTime_Len[0] = (DealTimeLen&0xFF00)>>4;
	pEcho_Union->Echo.DealTime_Len[1] =  DealTimeLen&0x00FF;
	pEcho_Union->Echo.UpDatFlag = UpDatFlagChl;
	pEcho_Union->Echo.UpDatFlag_Len[0] = (UpDatFlagLen&0xFF00)>>4;
	pEcho_Union->Echo.UpDatFlag_Len[1] =  UpDatFlagLen&0x00FF;	
	
	memcpy(pEcho_Union->Echo.Tid_Chl,Tid,sizeof(Tid));
	memcpy(pEcho_Union->Echo.DealData_Chl,DealData,sizeof(DealData));
	memcpy(pEcho_Union->Echo.DealTime_Chl,DealTime,sizeof(DealTime));
	memcpy(pEcho_Union->Echo.UpDatFlag_Chl,UpDatFlag,sizeof(UpDatFlag));
	
	memcpy(p,pEcho_Union,sizeof(pEcho_Union->EchoBuf));
	vPortFree(pEcho_Union);
	return  sizeof(pEcho_Union->EchoBuf);	
}

/**
  * @brief  获取取餐结构体数据
  * @param  NewState: new state of the Prefetch Buffer.
  *          This parameter  can be: ENABLE or DISABLE.
  * @retval None
  */
uint16_t get_tkmeal_data(uint8_t *p)
{
	TakeMeal_Union *pTakeMeal_Union;
	pTakeMeal_Union = (TakeMeal_Union *)pvPortMalloc(sizeof(pTakeMeal_Union));
	
	pTakeMeal_Union->TakeMeal.Tid = TidChl;
	pTakeMeal_Union->TakeMeal.Tid_Len[0] = (TidLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.Tid_Len[1] =  TidLen&0x00FF;
	pTakeMeal_Union->TakeMeal.Brw = BrwChl;
	pTakeMeal_Union->TakeMeal.Brw_Len[0] = (BrwLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.Brw_Len[1] =  BrwLen&0x00FF;
	pTakeMeal_Union->TakeMeal.Bno = BnoChl;
	pTakeMeal_Union->TakeMeal.Bno_Chl[0] = (BnoLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.Bno_Chl[1] =  BnoLen&0x00FF;
	pTakeMeal_Union->TakeMeal.DevArae = DevAraeChl;
	pTakeMeal_Union->TakeMeal.DevArae_Len[0] = (DevAraeLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.DevArae_Len[1] =  DevAraeLen&0x00FF;
	pTakeMeal_Union->TakeMeal.DevSite = DevSiteChl;
	pTakeMeal_Union->TakeMeal.DevSite_Len[0] = (DevSiteLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.DevSite_Len[1] =  DevSiteLen&0x00FF;
	pTakeMeal_Union->TakeMeal.Tradvol = TradvolChl;
	pTakeMeal_Union->TakeMeal.Tradvol_Len[0] = (TradvolLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.Tradvol_Len[1] =  TradvolLen&0x00FF;
	pTakeMeal_Union->TakeMeal.MealId = MealIdChl;
	pTakeMeal_Union->TakeMeal.MealId_Len[0] = (MealIdLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.MealId_Len[1] =  MealIdLen&0x00FF;
	pTakeMeal_Union->TakeMeal.MealQty = MealQtyChl;
	pTakeMeal_Union->TakeMeal.MealQty_Len[0] = (MealQtyLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.MealQty_Len[1] =  MealQtyLen&0x00FF;
	pTakeMeal_Union->TakeMeal.MealName = MealNameChl;
	pTakeMeal_Union->TakeMeal.MealName_Len[0] = (MealNameLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.MealName_Len[1] =  MealNameLen&0x00FF;
	pTakeMeal_Union->TakeMeal.MealPrice = MealPriceChl;
	pTakeMeal_Union->TakeMeal.MealPrice_Len[0] = (MealPriceLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.MealPrice_Len[1] =  MealPriceLen&0x00FF;
	pTakeMeal_Union->TakeMeal.PayType = PayTypeChl;
	pTakeMeal_Union->TakeMeal.PayType_Len[0] = (PayTypeLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.PayType_Len[1] =  PayTypeLen&0x00FF;
	pTakeMeal_Union->TakeMeal.Change = ChangeChl;
	pTakeMeal_Union->TakeMeal.Change_Len[0] = (ChangeLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.Change_Len[1] =  ChangeLen&0x00FF;
	pTakeMeal_Union->TakeMeal.RmnMealQty = RmnMealQtyChl;
	pTakeMeal_Union->TakeMeal.RmnMealQty_Len[0] = (RmnMealQtyLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.RmnMealQty_Len[1] =  RmnMealQtyLen&0x00FF;
	pTakeMeal_Union->TakeMeal.TkMealFlag = TkMealFlagChl;
	pTakeMeal_Union->TakeMeal.TkMealFlag_Len[0] = (TkMealFlagLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.TkMealFlag_Len[1] = TkMealFlagLen&0x00FF;
	pTakeMeal_Union->TakeMeal.PosDevNum = PosDevNumChl;
	pTakeMeal_Union->TakeMeal.PosDevNum_Len[0] = (PosDevNumLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.PosDevNum_Len[1] =  PosDevNumLen&0x00FF;
	pTakeMeal_Union->TakeMeal.PosTenantNum = PosTenantNumChl;
	pTakeMeal_Union->TakeMeal.PosTenantNum_Len[0] = (PosTenantNumLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.PosTenantNum_Len[1] =  PosTenantNumLen&0x00FF;
	pTakeMeal_Union->TakeMeal.PosBatchNum = PosBatchNumChl;
	pTakeMeal_Union->TakeMeal.PosBatchNum_Len[0] = (PosBatchNumLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.PosBatchNum_Len[1] =  PosBatchNumLen&0x00FF;
	pTakeMeal_Union->TakeMeal.PosUserNum = PosUserNumChl;
	pTakeMeal_Union->TakeMeal.PosUserNum_Len[0] = (PosUserNumLen&0xFF00)>>4;
	pTakeMeal_Union->TakeMeal.PosUserNum_Len[1] =  PosUserNumLen&0x00FF;
	pTakeMeal_Union->TakeMeal.Mac = MacChl;
	pTakeMeal_Union->TakeMeal.Mac_Len[0] = (MacLen&0xFF00)>>4;;
	pTakeMeal_Union->TakeMeal.Mac_Len[1] =  MacLen&0x00FF;
	
	memcpy(pTakeMeal_Union->TakeMeal.Tid_Chl,Tid,sizeof(Tid));
	memcpy(pTakeMeal_Union->TakeMeal.Brw_Chl,Brw,sizeof(Brw));
	memcpy(pTakeMeal_Union->TakeMeal.Bno_Chl,Bno,sizeof(Bno));
	memcpy(pTakeMeal_Union->TakeMeal.DevArae_Chl,DevArae,sizeof(DevArae));
	memcpy(pTakeMeal_Union->TakeMeal.DevSite_Chl,DevSite,sizeof(DevSite));
	memcpy(pTakeMeal_Union->TakeMeal.Tradvol_Chl,Tradvol,sizeof(Tradvol));
	memcpy(pTakeMeal_Union->TakeMeal.MealId_Chl,MealId,sizeof(MealId));
	memcpy(pTakeMeal_Union->TakeMeal.MealQty_Chl,MealQty,sizeof(MealQty));
	memcpy(pTakeMeal_Union->TakeMeal.MealName_Chl,MealName,sizeof(MealName));
	memcpy(pTakeMeal_Union->TakeMeal.MealPrice_Chl,MealPrice,sizeof(MealPrice));
	memcpy(pTakeMeal_Union->TakeMeal.PayType_Chl,PayType,sizeof(PayType));
	memcpy(pTakeMeal_Union->TakeMeal.Change_Chl,Change,sizeof(Change));
	memcpy(pTakeMeal_Union->TakeMeal.RmnMealQty_Chl,RmnMealQty,sizeof(RmnMealQty));
	memcpy(pTakeMeal_Union->TakeMeal.TkMealFlag_Chl,TkMealFlag,sizeof(TkMealFlag));
	memcpy(pTakeMeal_Union->TakeMeal.PosDevNum_Chl,PosDevNum,sizeof(PosDevNum));
	memcpy(pTakeMeal_Union->TakeMeal.PosTenantNum_Chl,PosTenantNum,sizeof(PosTenantNum));
	memcpy(pTakeMeal_Union->TakeMeal.PosBatchNum_Chl,PosBatchNum,sizeof(PosBatchNum));
	memcpy(pTakeMeal_Union->TakeMeal.PosUserNum_Chl,PosUserNum,sizeof(PosUserNum));
	memcpy(pTakeMeal_Union->TakeMeal.Mac_Chl,Mac,sizeof(Mac));
	
	memcpy(p,pTakeMeal_Union,sizeof(pTakeMeal_Union->TakeMealBuf));
	vPortFree(pTakeMeal_Union);
	return  sizeof(pTakeMeal_Union->TakeMealBuf);		
}
  
/**
  * @brief  将签到的数据指向sendbuff,这样就可以避免复制了
  * @param  NewState: new state of the Prefetch Buffer.
  *          This parameter  can be: ENABLE or DISABLE.
  * @retval None
  */

PACKET_STRUCT pcket_struct;   //帧的封装格式
void package_buff(uint16_t Request,uint8_t *request_buf)
{
	uint16_t crc_value;
	uint16_t contex_lenth=0;
	DATA_TIME_STRUCT DataTime;
	RTC_TimeShow(&DataTime);    //获取时间
	Time_Conver(&DataTime);		//转换时间数据到数组
	BatchNum_Conver(&DataTime,Brw);  //获取流水号
	pcket_struct.Stx = StxChl;  //获取帧头
	pcket_struct.Etx = EtxChl;  //获取帧尾
	pcket_struct.CmdReq[0]= ((Request&0xff00)>>8);  //获取请求
	pcket_struct.CmdReq[1]=  (Request&0x00ff);        
	memcpy(request_buf,&pcket_struct.Stx,sizeof(pcket_struct.Stx)); //复制单个使用地址 ：帧头
	memcpy(request_buf+1,pcket_struct.CmdReq,sizeof(pcket_struct.CmdReq)); //复制签到请求命令 
	//获取时间
	switch(Request)
	{
		case SignInReq:{
			contex_lenth = get_signin_data(request_buf+5); 
			//将签到用到的结构体中的数据写入sign_in_buf中，偏移5个位置，返回签到结构体的字节数 
		}break;
		case MealComparReq:{
			//SPI_Flash_Read(Meal_Union.MealBuf,MENU_RECORD_STAR,sizeof(Meal_Union));
			contex_lenth = get_mealcompar_data(request_buf+5); 
			//将签到用到的结构体中的数据写入sign_in_buf中，偏移5个位置，返回签到结构体的字节数 
		}break;
		case StatuUploadReq:{
			contex_lenth = get_statupload_data(request_buf+5); 
			//将签到用到的结构体中的数据写入sign_in_buf中，偏移5个位置，返回签到结构体的字节数     
		}break;
		case TkMealReq:{
			contex_lenth = get_tkmeal_data(request_buf+5); 
			//将签到用到的结构体中的数据写入sign_in_buf中，偏移5个位置，返回签到结构体的字节数        
		}break;
		case EchoReq:{
			contex_lenth = get_echo_data(request_buf+5); 
			//将签到用到的结构体中的数据写入sign_in_buf中，偏移5个位置，返回签到结构体的字节数 
		}break;
		default:break; 
	}
	pcket_struct.FrameLen[0]= ((contex_lenth&0xff00)>>8);
	pcket_struct.FrameLen[1]= (contex_lenth&0x00ff);
	memcpy(request_buf+3,pcket_struct.FrameLen,sizeof(pcket_struct.FrameLen)); //复制签到请求内容长度
	memcpy(request_buf+5+contex_lenth,&pcket_struct.Etx,sizeof(pcket_struct.Etx)); //复制单个使用地址 ：帧尾
	crc_value =  crc_ccitt(request_buf+1,contex_lenth+4);  //获取Crc的值，从帧头到帧尾
	pcket_struct.Crc[0]= ((crc_value&0xff00)>>8);
	pcket_struct.Crc[1]= (crc_value&0x00ff);
	memcpy(request_buf+6+contex_lenth,pcket_struct.Crc,sizeof(pcket_struct.Crc)); //复制CRC值    
}

/**
  * @brief  解码重服务器返回的数据
  * @param  NewState: new state of the Prefetch Buffer.
  *          This parameter  can be: ENABLE or DISABLE.
  * @retval None
  */
uint8_t decode_host_data(uint8_t *ptr)
{
	uint8_t err;
	static uint16_t res_cmd = 0,res_len = 0,crc_value=0,crc_value_calcu=0;
	if(ptr[0]!= StxChl)   return 1;  //判断帧头
	res_cmd = (ptr[1]<<8) + ptr[2];  //获取响应命令
	res_len = (ptr[3]<<8) + ptr[4];  //获取长度数据
	if(ptr[res_len+5]!= EtxChl)    return 2;  //判断帧尾
	crc_value = (ptr[res_len+6]<<8) + ptr[res_len+7];  //获取CRC16的值
	crc_value_calcu = GetCRC16(&ptr[1],res_len+4);//计算CRC的值
	if(crc_value != crc_value_calcu) return 3;  //判断CRC,校验数据的完整性 ,校验出错，不批匹配，不知道
	switch(res_cmd)
	{
		case SignInRes:
		{
			err = decode_signin_data(&ptr[5]);
		};break;
		case MealComparRes:
		{
			err = decode_mealcomp_data(&ptr[5]);
		};break;
		case StatuUploadRes:
		{
			err = decode_statupload_data(&ptr[5]);
		};break;
		case TkMealRes:
		{
			err = decode_getmeal_data(&ptr[5]);
		};break; 
		case EchoRes:
		{
			err = decode_echo_data(&ptr[5]);
		}
		default:
		{
			return 4;  //返回的命令请求错误
		}
	}
	return err;
}

/**
  * @brief  解码四种有服务器返回的数据
  * @param  NewState: new state of the Prefetch Buffer.
  *          This parameter  can be: ENABLE or DISABLE.
  * @retval None
  */
SIGN_IN_REQ_STRUCT sigin_req;
//extern Meal_Union meal_union;
//static uint8_t MealBuf[sizeof(Meal_Union.MealBuf)];
static uint8_t decode_signin_data(uint8_t *p)
{
	uint32_t byte_count=0;
	if(p[byte_count++]==NomiNameChl)
	{
		sigin_req.NomiName =NomiNameChl;
		sigin_req.NomiName_Len = (p[byte_count++]<<4);
		sigin_req.NomiName_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<sigin_req.NomiName_Len;i++)
		{
			sigin_req.NomiName_Chl[i] = p[byte_count++];
		}
	}
	if(p[byte_count++]==BnoChl)
	{
		sigin_req.Bno =BrwChl;
		sigin_req.Bno_Len = (p[byte_count++]<<4);
		sigin_req.Bno_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<sigin_req.Bno_Len;i++)
		{
			sigin_req.Bno_Chl[i] = p[byte_count++];
		}
		memcpy(Bno,sigin_req.Bno_Chl,BnoLen); //更新内部的批次号
	}
	if(p[byte_count++]==AppVerChl)
	{
		sigin_req.AppVer =AppVerChl;
		sigin_req.AppVer_Len = (p[byte_count++]<<4);
		sigin_req.AppVer_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<sigin_req.AppVer_Len;i++)
		{
			sigin_req.AppVer_Chl[i] = p[byte_count++];
		}
	}
	if(p[byte_count++]==ParaFileVerChl)
	{
		sigin_req.ParaFileVer =ParaFileVerChl;
		sigin_req.ParaFileVer_Len = (p[byte_count++]<<4);
		sigin_req.ParaFileVer_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<sigin_req.ParaFileVer_Len;i++)
		{
			sigin_req.ParaFileVer_Chl[i] = p[byte_count++];
		}
	}
	if(p[byte_count++]==BusiDatFileVerChl)
	{
		sigin_req.BusiDatFileVer =BusiDatFileVerChl;
		sigin_req.BusiDatFileVer_Len = (p[byte_count++]<<4);
		sigin_req.BusiDatFileVer_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<sigin_req.BusiDatFileVer_Len;i++)
		{
			sigin_req.BusiDatFileVer_Chl[i] = p[byte_count++];
		}
	}
	if(p[byte_count++]==UpDatFlagChl)
	{
		sigin_req.UpDatFlag =UpDatFlagChl;
		sigin_req.UpDatFlag_Len = (p[byte_count++]<<4);
		sigin_req.UpDatFlag_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<sigin_req.UpDatFlag_Len;i++)
		{
			sigin_req.UpDatFlag_Chl[i] = p[byte_count++];
		}
	}
	if(p[byte_count++]==MenuNumChl)
	{
		sigin_req.MenuNum =MenuNumChl;
		sigin_req.MenuNum_Len = (p[byte_count++]<<4);
		sigin_req.MenuNum_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<sigin_req.MenuNum_Len;i++)
		{
			sigin_req.MenuNum_Chl[i] = p[byte_count++];
		}
	}
	if(p[byte_count++]==MenuDetailChl)
	{
		sigin_req.MenuDetail =MenuDetailChl;
		sigin_req.MenuDetail_Len = (p[byte_count++]<<4);
		sigin_req.MenuDetail_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<sigin_req.MenuDetail_Len;i++)
		{
			sigin_req.MenuDetail_Chl[i] = p[byte_count];
			Meal_Union.MealBuf[i] = p[byte_count++];
		}
		//将餐品的数据写入flash 
		//SPI_Flash_Write(Meal_Union.MealBuf,MENU_RECORD_STAR,sizeof(Meal_Union));
		//SPI_Flash_Read(MealBuf,MENU_RECORD_STAR,sizeof(Meal_Union));
	}
	if(p[byte_count++]==AckChl)
	{
		sigin_req.Ack =AckChl;
		sigin_req.Ack_Len = (p[byte_count++]<<4);
		sigin_req.Ack_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<sigin_req.Ack_Len;i++)
		{
			sigin_req.Ack_Chl[i] = p[byte_count++];
		}
	}
	if(p[byte_count++]==CipherChl)
	{
		sigin_req.Cipher =CipherChl;
		sigin_req.Cipher_Len = (p[byte_count++]<<4);
		sigin_req.Cipher_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<sigin_req.Cipher_Len;i++)
		{
		 	sigin_req.Cipher_Chl[i] = p[byte_count++];
		}
	}
	return 0;
}

MEAL_COMPAR_REQ_STRUCT mealcompar_req;
MealDetailReq_Union  mealdetail_unoin;
static uint8_t decode_mealcomp_data(uint8_t *p)
{
	uint32_t byte_count=0;	
	if (p[byte_count++]==AckChl)
	{
		mealcompar_req.Ack =AckChl;
		mealcompar_req.Ack_Len = (p[byte_count++]<<4);
		mealcompar_req.Ack_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<mealcompar_req.Ack_Len;i++)
		{
			mealcompar_req.Ack_Chl[i] = p[byte_count++];
		}
	}
	if (p[byte_count++]==MealComparChl)
	{
		mealcompar_req.MealCompar =MealComparChl;
		mealcompar_req.MealCompar_Len = (p[byte_count++]<<4);
		mealcompar_req.MealCompar_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<mealcompar_req.MealCompar_Len;i++)
		{
			mealdetail_unoin.MealDetailReqBuf[i] = p[byte_count];
			mealcompar_req.MealCompar_Chl[i] = p[byte_count++];
		}
	}
	if (p[byte_count++]==MacChl)
	{
		mealcompar_req.Mac =MacChl;
		mealcompar_req.Mac_Len = (p[byte_count++]<<4);
		mealcompar_req.Mac_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<mealcompar_req.Mac_Len;i++)
		{
			mealcompar_req.Mac_Chl[i] = p[byte_count++];
		}
	}
	return 0;
}

STATU_UPLOAD_REQ_STRUCT statuupload_req;
static uint8_t decode_statupload_data(uint8_t *p)
{
	uint32_t byte_count=0;       
	if (p[byte_count++]==AckChl)
	{
		statuupload_req.Ack =AckChl;
		statuupload_req.Ack_Len = (p[byte_count++]<<4);
		statuupload_req.Ack_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<statuupload_req.Ack_Len;i++)
		{
			statuupload_req.Ack_Chl[i] = p[byte_count++];
		}
	}
	return 0;
}
TAKE_MEAL_REQ_STRUCT takemeal_req;
static uint8_t decode_getmeal_data(uint8_t *p)
{
	uint32_t byte_count=0;
	if (p[byte_count++]==AckChl)
	{
		takemeal_req.Ack =AckChl;
		takemeal_req.Ack_Len = (p[byte_count++]<<4);
		takemeal_req.Ack_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<takemeal_req.Ack_Len;i++)
		{
			takemeal_req.Ack_Chl[i] = p[byte_count++];
		}
	}
	if (p[byte_count++]==MacChl)
	{
		takemeal_req.Mac =MacChl;
		takemeal_req.Mac_Len = (p[byte_count++]<<4);
		takemeal_req.Mac_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<takemeal_req.Mac_Len;i++)
		{
			takemeal_req.Mac_Chl[i] = p[byte_count++];
		}
	}	
	return 0;
}

ECHO_REQ_STRUCT echo_req;
static uint8_t decode_echo_data(uint8_t *p)
{
	uint32_t byte_count=0;
	if (p[byte_count++]==DealDataChl)
	{
		echo_req.DealData =DealDataChl;
		echo_req.DealData_Len = (p[byte_count++]<<4);
		echo_req.DealData_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<echo_req.DealData_Len;i++)
		{
			echo_req.DealData_Chl[i] = p[byte_count++];
		}
	}
	if (p[byte_count++]==DealTimeChl)
	{
		echo_req.DealTime =DealTimeChl;
		echo_req.DealTime_Len = (p[byte_count++]<<4);
		echo_req.DealTime_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<echo_req.DealTime_Len;i++)
		{   
			echo_req.DealTime_Chl[i] = p[byte_count++];
		}
	}
	if (p[byte_count++]==AckChl)
	{
		echo_req.Ack =AckChl;
		echo_req.Ack_Len = (p[byte_count++]<<4);
		echo_req.Ack_Len += (p[byte_count++]); 
		for(uint8_t i=0;i<echo_req.Ack_Len;i++)
		{
			echo_req.Ack_Chl[i] = p[byte_count++];
		}
	}
	return 0;
}
/**
  * @brief  向服务器发送签到数据
  * @param  NewState: new state of the Prefetch Buffer.
  *          This parameter  can be: ENABLE or DISABLE.
  * @retval None
  */
xQueueHandle xQueue;
long lReceivedValue;
/*-----------------------------------------------------------------------------------*/
static void tcpclient_thread(void *arg)
{
	struct netconn *tcp_clientconn;
	struct ip_addr ServerIPaddr;

	struct   netbuf *inbuf;
	uint8_t* buf;
	uint16_t buflen;	
	
	portBASE_TYPE xStatus;
	const portTickType xTicksToWait =  10 / portTICK_RATE_MS;
	static err_t err,recv_err;
	static uint8_t  send_success_flag = 0x00;
	LWIP_UNUSED_ARG(arg);
	
	/* add SERVER_IP to ServerIPaddr*/
	IP4_ADDR( &ServerIPaddr, SERVER_IP_ADDR0, SERVER_IP_ADDR1, SERVER_IP_ADDR2, SERVER_IP_ADDR3 );

	xQueue = xQueueCreate( 10, sizeof( long ) );

  #if 1
	while(1)
	{
	  /* Create a new connection identifier. */
		tcp_clientconn = netconn_new(NETCONN_TCP);
		if (tcp_clientconn!=NULL)
		{  
			/*built a connect to server*/
			err = netconn_connect(tcp_clientconn,&ServerIPaddr,SERVER_PORT);
			if (err != ERR_OK)  netconn_delete(tcp_clientconn); 
			else if (err == ERR_OK)
			{
				/*timeout to wait for new data to be received <Avoid death etc.> */
				netconn_set_sendtimeout(tcp_clientconn,10);
				netconn_set_recvtimeout(tcp_clientconn,800);

				while(1)
				{
					//if((tcp_client_flag & SignInFlag) == SignInFlag)  
					xStatus = xQueueReceive( xQueue,
											 &lReceivedValue,
											 xTicksToWait );
					if(xStatus == pdPASS)
					//if(lReceivedValue>0)                        
					{
						//netconn_set_sendtimeout(tcp_clientconn,10);
						//printf("xStatus = pdPASS.\r\n");
						switch(lReceivedValue)
						{
							case SignInReq:{
								auto uint8_t sign_in_buf[Totoal_SignIn_Lenth+ 8]={0};  //使用auto，该类具有自动存储期
								package_buff(SignInReq,sign_in_buf);   //将要发送的数据填入sendbuf        
								err=netconn_write(tcp_clientconn,\
								sign_in_buf,sizeof(sign_in_buf),\
								NETCONN_COPY); 
								if(err != ERR_OK)  
									printf(" SignInReq erro code is ERR_TIMEOUT \r\n");      
							};break;
							case MealComparReq:{
								auto uint8_t meal_compar_buf[Totoal_MealCompar_Lenth+ 8]={0};  //使用auto，该类具有自动存储期
								package_buff(MealComparReq,meal_compar_buf);   //将要发送的数据填入sendbuf        
								err=netconn_write(tcp_clientconn,\
							 	meal_compar_buf,sizeof(meal_compar_buf),\
								NETCONN_COPY); 
								if(err != ERR_OK)  
									printf("MealComparReq erro code is %d\r\n",err);                     
							};break;
							case StatuUploadReq:{
								auto uint8_t statu_upload_buf[Totoal_StatuUpload_Lenth+ 8]={0};  //使用auto，该类具有自动存储期
								package_buff(StatuUploadReq,statu_upload_buf);   //将要发送的数据填入sendbuf        
								err=netconn_write(tcp_clientconn,\
								statu_upload_buf,sizeof(statu_upload_buf),\
								NETCONN_COPY); 
								if(err != ERR_OK)  
									printf("StatuUploadReq erro code is %d\r\n",err);                               
							};break;
							case TkMealReq:{
								auto uint8_t tk_meal_buf[Tk_Meal_Lenth+ 8]={0};  //使用auto，该类具有自动存储期
								package_buff(TkMealReq,tk_meal_buf);   //将要发送的数据填入sendbuf    
								err=netconn_write(tcp_clientconn,\
								tk_meal_buf,sizeof(tk_meal_buf),\
								NETCONN_COPY); 
								if(err != ERR_OK)  
									printf("StatuUploadReq erro code is %d\r\n",err);      				
							};break;
							case EchoReq:{
								auto uint8_t echo_buf[Totoal_Echo_Lenth+ 8]={0};  //使用auto，该类具有自动存储期
								package_buff(EchoReq,echo_buf);   //将要发送的数据填入sendbuf    
								err=netconn_write(tcp_clientconn,\
								echo_buf,sizeof(echo_buf),\
								NETCONN_COPY); 
								if(err != ERR_OK)  
									printf("StatuUploadReq erro code is %d\r\n",err);      	
							};break;
							default:break;
						}
						send_success_flag = 0x01; 
						//接受一个Queue后，进行计时，2S后无数据判断失败
					}
					//进入接听状态
					if(ERR_OK ==  netconn_listen_with_backlog(tcp_clientconn,2))
					{
						recv_err = netconn_recv(tcp_clientconn, &inbuf);
						if (recv_err == ERR_OK)
						{
							if (netconn_err(tcp_clientconn ) == ERR_OK)
							{ 
								netbuf_data(inbuf, (void**)&buf, &buflen);
								decode_host_data(buf);
								netbuf_delete(inbuf);
								send_success_flag = 0x00; 
								lReceivedValue = 0 ; 
							}
							else
							{
								printf("recv_err != ERR_OK \r\n");
							}
						}
						else if((recv_err == ERR_TIMEOUT)&&(send_success_flag == 0x01)) 
						{
							send_success_flag = 0x00;
							printf("recv_err == %d\r\n",recv_err);
							netconn_close(tcp_clientconn);
							netbuf_delete(inbuf);
							netconn_delete(tcp_clientconn);
							break;
						}
					}
				}
			}
		}     
	}
#endif
}

/*-----------------------------------------------------------------------------------*/

void Tcpclient_Init(void)
{
  sys_thread_new("tcpserv", tcpclient_thread, NULL, DEFAULT_THREAD_STACKSIZE * 2, TCPCLIENT_THREAD_PRIO);
}
/*-----------------------------------------------------------------------------------*/

#endif /* LWIP_NETCONN */


