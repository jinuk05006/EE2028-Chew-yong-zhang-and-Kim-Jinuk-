//include all libraries we are using
#include "main.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_accelero.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_tsensor.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_psensor.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_hsensor.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_gyro.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_magneto.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01.h"
#include "stdio.h"
#include "string.h"
//functions we are going to call
extern void initialise_monitor_handles(void);  // for semi-hosting support (printf)
static void MX_GPIO_Init(void);
static void UART1_Init(void);
static void GatheredData(void);
static void EXPLORER_MODE(void);
static void BATTLE_MODE(void);
static void WARNING_MODE(void);
static void single_press(void);
static void check_ths(float hum_data
		,float rms_mag_data
		,float rms_gyro_data
		,float temp_data
		,float bruh //accel_data[2]
		,float pressure_data);
static void monitor(void);

void SystemClock_Config(void);

//global variables and constants
#define exploring 0
#define warning 1
#define battle 2

int sec_counter;
int mode = 0;
int previous_mode;
int battery = 10;
int flag_pb = 0;
int flag_sp = 0;
int flag_dp = 0;
char message_print[32];
char big_message_print[128];

uint32_t t1,t2,texp,tbat;

UART_HandleTypeDef huart1;

//Our beloved GPIO EXTI callback function
HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == BUTTON_EXTI13_Pin)
    {
      if(flag_pb == 1){
        flag_pb = 2;
        t2 = uwTick;
      }
      if(flag_pb == 0){
        flag_pb = 1;
        t1 = uwTick;
      }

      while(1){
        if(flag_pb == 2 && t2 -t1<= 1000){
          flag_dp = 1;
          flag_pb = 0;
          flag_sp = 0;
          break;
        }
       if (flag_pb == 2 && t2 - t1> 1000){
          flag_sp = 1;
          flag_pb = 0;
          flag_dp = 0;
          break;
        }
        break;
      }
    }



}


int main(void)
{
//initialise everything here
	
	initialise_monitor_handles(); // for semi-hosting support (printf)
	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();
	MX_GPIO_Init();
	UART1_Init();
	/* Peripheral initializations using BSP functions */
	BSP_ACCELERO_Init();
	BSP_TSENSOR_Init();
	BSP_PSENSOR_Init();
	BSP_HSENSOR_Init();
	BSP_GYRO_Init();
	BSP_MAGNETO_Init();
	monitor();

	//infinite loop using conditionals to funnel the guy through different modes
	while(1){
		if(mode == exploring && flag_pb == 0 && flag_sp == 0 && flag_dp == 0){
			EXPLORER_MODE();
		}
		if(mode == battle && flag_dp == 1 ){
			BATTLE_MODE();
		}
		if (mode == exploring && flag_dp == 1){
			EXPLORER_MODE();
		}
		if(mode == battle && flag_pb == 0 && flag_sp == 0 && flag_dp == 0){
			BATTLE_MODE();
		}

		if (mode == warning ){
			WARNING_MODE();
		}
	}
}

static void EXPLORER_MODE(void)
{
	//initialize all the stuff here
	mode = exploring;
	previous_mode = mode;
	flag_dp = 0;
	flag_sp = 0;
	sprintf(message_print,"I'm exploring\r\n");
	HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF);
	int exp_counter=0;
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14,1);

	//while loop with two break conditions
	while(1){
		texp = uwTick;
		if(exp_counter%10==0){
		//this line gives the flag_dp ==1 and mode == warning
			GatheredData();
		}
		if(flag_dp == 1 && mode == warning){
			mode = warning;
			break;
		}
		//this line is to change mode with double press
		if(flag_dp == 1 && mode == exploring){
			mode = battle;
			break;
		}
		while(1){
			if((uwTick- texp)==100){
				exp_counter++;
				break;
			}
		}
	}
}

static void BATTLE_MODE(void)
{
	//Initializing stuff
	mode = battle;
	previous_mode = battle;
	flag_dp = 0;
	flag_sp = 0;
	sec_counter = 0;

	sprintf(message_print,"I am in battle\r\n");
	HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF);

	//while not double pressed
	while(flag_dp == 0){
		single_press(); //check for single press

		if(sec_counter%100 == 0 && sec_counter != 0){ //every 1 second
			GatheredData(); //threshhold check inside if threshhold exceeded then flag_dp = 1 and break
			}
		if(sec_counter%50 == 0){ //every half a second
			HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
			}

		if(sec_counter%500 == 0 && sec_counter != 0){ //every five second
			if(battery-2 >= 0){
				battery = battery - 2;
				monitor();
				sprintf(big_message_print,"bang bang bang!\r\nCurrent Battery level: %d/10\r\n",battery);
				HAL_UART_Transmit(&huart1, (uint8_t*)big_message_print, strlen(big_message_print),0xFFFF);
			}else{
				monitor();
				sprintf(message_print,"Out of battery!\r\n");
				HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF);
			}
		}
		if(flag_dp == 1 && mode == warning){ //from gathereddata few lines back
			mode = warning;
			break;
		}
		if(flag_dp ==1 && mode == battle){ //from double press
			mode = exploring;
			break;
		}
		sec_counter++;
		tbat= uwTick;
		while(1){ //our 0.01s delay
			if(uwTick-tbat==10){
				break;
			}
		}
	}
}
static void WARNING_MODE(void)
{	//initialize stuff
	mode = previous_mode;
	flag_sp = 0;
	flag_dp = 0;
	sec_counter = 0;
	
	//while single press is 0
	while(flag_sp == 0){
		mode = warning;
		single_press(); //check for single press. if single press = 1, break
		uint32_t t1=uwTick;
		if(sec_counter%100==0){
			sprintf(message_print,"WARNING mode: SOS!\r\n");
			HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF);
		}
		if(sec_counter%16==0){
		  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
		}
		while(1){
			if((uwTick-t1)==10){
				sec_counter++;
				break;
		  	  	  }
				}
		}
	mode = previous_mode; //return to previous mode
	flag_sp = 0; //set single press back to 0 so the funnel in main can funnel the robot into previous mode
}

//check for single press
static void single_press(void){
	while(flag_pb == 1){ //while button is press else skip the whole thing
		if(uwTick -t1>1000){ //if nothing happen after the first press
        flag_pb = 0;
        flag_sp = 1; // single press =1
        if(mode == battle){ //if battle mode and
          if(battery<10){ //battery less than 10 then
          battery++; //charge battery.
          monitor(); //led monitor change accordingly
          sprintf(big_message_print,"CHARGING!\r\nCurrent Battery Level: %d/10\r\n",battery); //battery level
          HAL_UART_Transmit(&huart1, (uint8_t*)big_message_print, strlen(big_message_print),0xFFFF);
          }else{ //if battery is 10 
        	 monitor(); // monitor shows f
            sprintf(message_print,"Battery is full!\r\n"); 
            HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF);
          }
        }
      }
  }
}

//this is where we gather data and check for threshhold limit of the data
static void GatheredData(void){
	float hum_data; 
	hum_data = BSP_HSENSOR_ReadHumidity(); //humidity

	float mag_data[3];
	float rms_mag_data;
	int16_t mag_data_i16[3] = { 0 };
	BSP_MAGNETO_GetXYZ(mag_data_i16);
	mag_data[0] = (float)mag_data_i16[0]/100.0f;
	mag_data[1] = (float)mag_data_i16[1]/100.0f;
	mag_data[2] = (float)mag_data_i16[2]/100.0f;
	rms_mag_data = (sqrt((mag_data[0]*mag_data[0])+(mag_data[1]*mag_data[1])+(mag_data[2]*mag_data[2]))); //rms value of magnetometer

	float gyro_data[3];
	float rms_gyro_data;
	int16_t gyro_data_i16[3] = { 0 }; 

	BSP_GYRO_GetXYZ(gyro_data_i16);
	gyro_data[0] = (float)gyro_data_i16[0]/100.0f;
	gyro_data[1] = (float)gyro_data_i16[1]/100.0f;
	gyro_data[2] = (float)gyro_data_i16[2]/100.0f;
	rms_gyro_data = (sqrt((gyro_data[0]*gyro_data[0])+(gyro_data[1]*gyro_data[1])+(gyro_data[2]*gyro_data[2]))); //rms value of gyroscope

	float temp_data;
	temp_data = BSP_TSENSOR_ReadTemp(); //temperature

	float accel_data[3];
	int16_t accel_data_i16[3] = { 0 };      // array to store the x, y and z readings.
	BSP_ACCELERO_AccGetXYZ(accel_data_i16);    // read accelerometer
		// the function above returns 16 bit integers which are 100 * acceleration_in_m/s2. Converting to float to print the actual acceleration.
	accel_data[0] = (float)accel_data_i16[0] / 100.0f;
	accel_data[1] = (float)accel_data_i16[1] / 100.0f;
	accel_data[2] = (float)accel_data_i16[2] / 100.0f;
	float bruh = accel_data[2]; //bruh is z axis accel data

	float pressure_data;
	pressure_data = BSP_PSENSOR_ReadPressure()/10.0f; //pressure data

	check_ths(hum_data,rms_mag_data, rms_gyro_data, temp_data,bruh,pressure_data); //check threshhold. 2 flags for exploring and 1 flag for battle will trigger warning mode

	//for battle mode
	if(mode == battle){
		sprintf(big_message_print,"T: %f deg cel, P:%f kPa, H:%f %%, A:%f M s^-2, G:%f dps, M:%f gauss\r\n", temp_data, pressure_data,hum_data,accel_data[2],rms_gyro_data,rms_mag_data);
		HAL_UART_Transmit(&huart1, (uint8_t*)big_message_print, strlen(big_message_print),0xFFFF);
	}
	//for exploration mode
	else if(mode == exploring){
		sprintf(big_message_print,"G:%f dps, M:%f gauss, P:%f kPa, H:%f %% \r\n",rms_gyro_data,rms_mag_data,pressure_data,hum_data);
		HAL_UART_Transmit(&huart1, (uint8_t*)big_message_print, strlen(big_message_print),0xFFFF);
	}

}
//checking threshhold
static void check_ths(float hum_data,float rms_mag_data,float rms_gyro_data,float temp_data,float bruh,float pressure_data){
	int flag = 0; //this one for exploring mode
	int sens_flag = 0; //this one for battle mode

	if(hum_data<= 10.0f){ //10 percent is like super dry. maybe the robot stuck in fridge or it has suddenly appeared in desert.
		flag++;
	}
	if(rms_mag_data>=35.0f){ 
		flag++;
	}
	if(rms_gyro_data>400.0f){ //check if it's toppling 
		flag++;
	}
	if(temp_data>=35.0f || temp_data<= 10.0f){//35 is quite hot in my opinion and 10 degrees might be too cold for the robot since he is naked
		sens_flag++;
	}
	if(bruh<= 0.0f || bruh>= 20.0){//accelerometer. 0 means he has toppled to his side and 20 means something is really wrong.
		sens_flag++;
	}
	if(pressure_data>=110.0f || pressure_data<=70.0f){//either he has gone underwater or gone super high up in the air.
		flag++;
	}
	 if(previous_mode==battle){ //case for battle mode
	    if((flag+sens_flag)>=1){
	      mode= warning; //change mode to warning
	      flag_dp =1; //double press flag to funnel the program from main to warning
	      sprintf(message_print,"%d anomalous data detected.\r\n",(flag+sens_flag));
	      HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF);
	    }else{
	      mode=battle; //else stay warning
	      sprintf(message_print,"%d anomalous data detected.\r\n",(flag+sens_flag));
	      HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF);
	    }
	  }
	  else if(previous_mode==exploring){ //case for exploration mode
	    if(flag>=2){
	      mode=warning; //change mode to warning 
	      flag_dp =1; //double press flag to funnel the program from main to warning
	      sprintf(message_print,"%d anomalous data detected.\r\n",flag);
	        HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF);
	       }else{
	      mode=exploring; //else stay exploring
	      sprintf(message_print,"%d anomalous data detected.\r\n",flag);
	        HAL_UART_Transmit(&huart1, (uint8_t*)message_print, strlen(message_print),0xFFFF);
	       	 	 }
	  }
}
//extra stuff for configuration
static void UART1_Init(void)
{
    /* Pin configuration for UART. BSP_COM_Init() can do this automatically */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);


/* Configuring UART1 */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
      while(1);
    }

}

static void monitor(void){
  if(battery==0){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 1); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 1); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 1); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 1); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 1); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 1); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 0); //D11=G
  }
  if(battery==1){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 1); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 0); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 0); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 1); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 0); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 0); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 0); //D11=G
  }
  if(battery==2){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 0); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 1); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 1); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 1); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 1); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 0); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 1); //D11=G
  }
  if(battery==3){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 1); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 1); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 0); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 1); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 1); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 0); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 1); //D11=G
  }
  if(battery==4){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 1); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 0); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 0); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 1); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 0); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 1); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 1); //D11=G
  }
  if(battery==5){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 1); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 1); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 0); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 0); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 1); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 1); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 1); //D11=G
  }
  if(battery==6){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 1); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 1); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 1); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 0); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 1); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 1); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 1); //D11=G
  }
  if(battery==7){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 1); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 0); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 0); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 1); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 1); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 0); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 0); //D11=G
  }
  if(battery==8){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 1); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 1); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 1); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 1); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 1); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 1); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 1); //D11=G
  }


if(battery==9){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 1); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 1); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 0); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 1); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 1); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 1); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 1); //D11=G
  }
  if(battery==10){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , 1); //D4=dot
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , 0); //D5=C
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , 0); //D6=D
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , 1); //D7=E
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , 0); //D8=B
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , 1); //D9=A
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , 1); //D10=F
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , 1); //D11=G
  }

}

static void MX_GPIO_Init(void)

{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE(); //Enable AHB2 Bus for GPIOB
  __HAL_RCC_GPIOC_CLK_ENABLE(); //ENable AHB2 Bus for GPIOC

  HAL_GPIO_WritePin(GPIOB, LED2_Pin, GPIO_PIN_RESET); // Reset the LED2_Pin as 0
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 , GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 , GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 , GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 , GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 , GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 , GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 , GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 , GPIO_PIN_RESET);
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // Configuration of LED2_Pin (GPIO-B Pin-14) as GPIO output
  GPIO_InitStruct.Pin = LED2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  //Configuration of Monitor
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Configuration of BUTTON_EXTI13_Pin (G{IO-C Pin-13)as AF
  GPIO_InitStruct.Pin = BUTTON_EXTI13_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  // Enable NVIC EXTI line 13
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
  HAL_NVIC_EnableIRQ(SysTick_IRQn);
  HAL_NVIC_SetPriority(SysTick_IRQn, 1, 1);
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);


}
