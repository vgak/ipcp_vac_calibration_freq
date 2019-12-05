//***********************************************
#ifndef __I2C_CMD_H
#define __I2C_CMD_H
//***********************************************
// Пакет начинаетс
#define PACKET_START		0x1B
// далее идёт длина пакета один байт (длина пакета без CRC)
// затем байт адреса устройства
// далее команда для устройства
// параметры если они нужны для команды
// завершает пакет CRC
/*
 Расчет контрольной суммы
 unsigned char crc;
 . . .
 . . .
 crc = 0;
 for(i=0;i<long_buf;i++) scrc(buf[i]);
 buf[long_buf] = crc;
 . . .
 . . .
void scrc(char inch)
{
	char b;
	char c_rc = crc;
	for(b=0;b<8;b++){
  		if((inch ^ c_rc) & 0x01) c_rc = 0x80 | ((c_rc^0x18) >> 1);
  		else 			 	 	 c_rc = c_rc >> 1;
  		inch >>= 1;
 	}
	crc = c_rc;
}
*/
//*************************************************
// Идентификаторы устройств.
#define HUB_ID	    		0x10		        // Хаб
#define MONO_ID	    		0x20		        // Монохроматор
#define MIRROR_ID			0x22		        // Мотор Зеркало
#define FRAME_ID        	0x24                // Мотор рамки
#define FILTR_ID        	0x26                // Мотор фильтров
#define S256_MONO_ID		0x28		        // Монохроматор Спектрана 256
#define SLIT_ID         	0x2A                // Мотор щели
#define ADS1252_ID 			0x30				// АЦП на основе ADS1252
#define PLM_ADS1250_ID 		0x32				// ПЛМ + АЦП на основе ADS1250
#define S256_ADC_ID		 	0x34				// АЦП на основе ADS1252 для СФ256
#define IK400_ADC_ID	 	0x36				// АЦП на основе ADS1252 с инфракрасным датциком и модулятором 200-400Гц
#define SYNCHRO_ADS1252_ID      0x70		//АЦП на основе ADS1252 c поддержкой синхродетектора
//***********************************************************************************************
// Адрес хаба
#define	HUB_ADDR		0x8A
// стартовый адрес устройств
#define	START_ADDR	0xFA
//***********************************************************************************************
// Команды Хабу.....
// Старт системы и раздача адресов
// Команда сброса системы и раздачи адресов. Подаётся на адрес HUB_ADR.
#define	CMD_HUB_RESET_SYSTEM	0x00
	// На эту команду сначало приходит пакет от хаба
	#define	RET_HUB_HOME_FIND		0x00
	//далее, приходять пакеты RET_DEVICE_VERSION от установленых в системе устройств
	// Назначаемые адреса 0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,0x90,0xA0,0xB0,0xD0,0xE0,0xF0
	// И завершает пакет
	#define	RET_HUB_END_FIND		0x01


// Запросить с хаба версию программы
#define	CMD_HUB_GET_VERSION		0x01
	// возврат
	#define	RET_HUB_VERSION			0x02
	// RETURN PACK: HUB_ADDR,RET_HUB_VERSION,HUB_ID,VERSION,SUB_VERSION,LONG_SERIAL_NUMBER
#define LONG_SERIAL_NUMBER	8

	// устройство не ответило
	#define RET_ADDR_ERROR			0x03

	// запрос PC о работе программы
	#define CMP_PROGRAMM_OK			0x04
		// ответ от РС
		#define RET_PC_OK		0x02

//***********************************************************************************************
// Общие команды для всех устройств

// После сброса устройство устанавливает START_ADR, и ждёт команды на смену адреса
#define CMD_DEVICE_CHANGE_ADDR	    0x01
		// данными к нему идёт новый адрес, и коды исключения ошибки
		// PACK: DEVICE_ADR,CMD_CHANGE_ADR,NEW_ADR,0x55,0xAA
	// На что устрайство отвечает своим ID и переустанавливает I2C адрес, или CMD_BAD
	#define RET_DEVICE_VERSION			0x01
		// RETURN PACK: DEVICE_ADR_I2C,DEVICE_VERSION,ID,VERSION,SUB_VERSION,+8byte серийный номер
	// или если формат команды не правильный ответит RET_DEVICE_CMD_BAD


// Получение версии устройства
#define CMD_DEVICE_GET_VERSION	0x02
		// PACK: DEVICE_ADDR,CMD_DEVICE_GET_VERSION
		// RETURN PACK: DEVICE_ADR,RET_DEVICE_VERSION,ID,VERSION,SUB_VERSION,+8byte серийный номер

// команда устройством не понята
		#define RET_DEVICE_CMD_BAD	0xFF

// Команда установить серийный номер
// LONG_SERIAL_NUMBER байт ANSI серийный номер
#define CMD_SERIAL_NUMBER		0x03
	// сразу ответит
	#define	RET_OK                     0x02
	// или если формат команды не правильный ответит RET_DEVICE_CMD_BAD
//***********************************************************************************************
// Команды для монохроматора
//***********************************************************************************************

// Команда встать на "НУЛЬ"
// параметров нет
#define CMD_MONO_SET_ZERO				0x10
	// ответит когда доедет до "НУЛЯ"
	#define RET_MONO_ZERO					0x10
	// или если за 6 попыток не стронится с места то ответит RET_MONO_BAD + CODE_ERROR(1 byte)


// Команда встать на шаг
// 4 байта - unsigned long требуемый шаг
#define CMD_MONO_SET_STEP  				0x11
	// ответит когда доедет до требуемого шага
	#define RET_MONO_STEP					0x11
	// или если формат команды не правильный ответит RET_DEVICE_CMD_BAD
	// или если была потеря позиции ответит RET_MONO_NO_ZERO
	// или ответит RET_MONO_BAD + CODE_ERROR(1 byte)


// Команда получить текущий шаг
#define CMD_MONO_GET_CURRENT_STEP		0x12
	// сразу ответит
	#define RET_MONO_CURRENT_STEP			0x12
	// и 4 байта - unsigned long текущий шаг


// Команда встать на длину волны
// 4 байта - float требуемая длина волны
#define CMD_MONO_SET_LONGWAVE			0x13
	// ответит когда доедет до требуемой длины волны
	#define RET_MONO_OK		  				0x13


// Команда получить текущую длину волны
#define CMD_MONO_GET_CURRENT_LONGWAVE	0x14
	// сразу ответит
	#define RET_MONO_CURRENT_LONGWAVE		0x14
	// и 4 байта - unsigned long текущая длина волны


#define LONG_NAME_GRATE 10
// Команда установить коэфициенты монохроматора
// 1 байт - индекс решётки
// 4 байта - float начальная длина волны
// 4 байта - float дисперси
// LONG_NAME_GRATE байт ANSI имя решётки
#define CMD_MONO_SET_FACTOR				0x15
	// сразу ответит RET_MONO_OK


// Команда получить коэфициенты монохроматора
// 1 байт - индекс решётки
#define CMD_MONO_GET_FACTOR 			0x16
	// сразу ответит, что команда выполнена успешно
	#define RET_MONO_FACTOR 				0x15
	// 4 байта - float начальная длина волны
	// 4 байта - float дисперси
	// LONG_NAME_GRATE байт ANSI имя решётки


// Команда получить индекс активной решётки
#define CMD_MONO_GET_CURRENT_FACTOR		0x17
	// сразу ответит, что команда выполнена успешно
	#define RET_MONO_CURRENT_FACTOR			0x16
	// 1 байт - индекс активной решётки

// Команда установить индекс активной решётки до этого действует индекс активной решётки полученный с датчика
// 1 байт - индекс активной решётки
#define CMD_MONO_SET_CURRENT_FACTOR	 	0x18

// Команда установить скорость мотора
// 2 байта - word время одного шага (1ед. = 16.0 мкС.)
#define CMD_MONO_SET_SPEED				0x19
	// сразу ответит RET_MONO_OK


// Команда получить максимальную скорость
#define CMD_MONO_GET_SPEED				0x1A
	// сразу ответит, что команда выполнена успешно
	#define RET_MONO_SPEED					0x17
	// 2 байта - word время одного шага (1ед. = 16.0 мкС.) максимальной скорости до разгона


// Команда установить эхо в шагах (действует только на следующую команду установки положения, иначе это нет)
// 4 байта - unsigned long требуемый шаг через который MONO ответит пакетом RET_MONO_CURRENT_STEP
#define CMD_MONO_SET_ECHO_STEP			0x1B
	// сразу ответит RET_MONO_OK


// Команда установить адрес эхо (действует только на следующую команду установки положения, иначе адрес хаба)
// 1 байта - адрес устройства на которое MONO ответит пакетом RET_MONO_CURRENT_STEP
#define CMD_MONO_SET_ECHO_ADDR			0x1C
	// сразу ответит RET_MONO_OK

// Команда останова
#define CMD_MONO_STOP					0x1D
	// после останова ответит
	#define	RET_MONO_STOP					0x18

// Команда установить серийный номер
// LONG_SERIAL_NUMBER байт ANSI серийный номер
#define CMD_MONO_SERIAL_NUMBER			0x1E
	// сразу ответит RET_MONO_OK

#define CMD_MONO_SAVE_STEP				0x1F


	#define RET_MONO_BAD	  				0x19
	#define RET_MONO_NO_ZERO  				0x1A

// Задать скорость с которой он уверено трогается, то есть максимум без разгона
// 2 байта - WORD скорость
#define CMD_MONO_SET_DEF_SPEED  		0x20
	// сразу ответит RET_MONO_OK
// Получить скорость с которой он уверено трогается, то есть максимум без разгона
#define CMD_MONO_GET_DEF_SPEED  		0x21
	#define RET_MONO_DEF_SPEED 				0x1c
	// Ответит RET_MONO_DEF_SPEED + 2 байта WORD скорость

// Задать количество шагов разгона
// 1 байта - количество шагов
#define CMD_MONO_SET_STEP_RAZG  		0x22
	// сразу ответит RET_MONO_OK
// Получить количество шагов разгона
#define CMD_MONO_GET_STEP_RAZG  		0x23
	#define RET_MONO_STEP_RAZG 				0x1D
	// Ответит RET_MONO_DEF_SPEED + 1 байта - количество шагов

// Если на MONO приходит пакет не от хаба, MONO дабавит
 //к нему RET_MONO_CURRENT_STEP и пошлёт на установленный адрес ECHO
// если адрес ECHO не установлен, то это адрес HUB_ADDR
//***********************************************************************************************
// Параметры для монохроматора
#define PARM_MONO_MIN_QUANTUMS_STEP		20    // Минимальное количество квантов для скорости
#define PARM_MONO_MAX_QUANTUMS_STEP		65535   // Максимальное количество квантов для скорости
#define PARM_MONO_QUANTUM_STEP			16.0  	// время одного кванта скорости в мкС
#define	PARM_MONO_MAX_STEP				73000L


//***********************************************************************************************
// Команды для АЦП на основе ADS1252
//***********************************************************************************************

// Команда получить N измерений
// параметры 2 байта, word сколько сделать в сумме измерений
#define CMD_ADS1252_N_MEASURE			0x20
	// ответит когда сделает N измерений
	#define RET_ADS1252_N_MEASURE			0x20
	// в параметрах float сумма N измерений, word N.


// Команда запустить измерение
#define CMD_ADS1252_START_MEASURE		0x21
	// сразу ответит
	#define	RET_ADS1252_OK					0x21

// Команда остановить измерение
#define CMD_ADS1252_STOP_MEASURE		0x22
	// сразу ответит
	#define	RET_ADS1252_STOP				0x22

// Команда запустить измерение
// 4 байта - unsigned long измерений (ECHO пакетов)
#define CMD_ADS1252_START_ECHO			0x23
	// ответит по завершению N ECHO
	#define	RET_ADS1252_END_ECHO			0x23
// Команда установить эхо в N измерений (действует постоянно)
// 2 байта - word N измерений через который ADS1252 ответит пакетом RET_ADS1252_N_MEASURE
#define CMD_ADS1252_SET_ECHO_MEASURE	0x24
	// сразу ответит RET_ADS1252_OK


// Команда установить адрес эхо (действует только на следующую
// команду CMD_ADS1252_START_MEASURE, иначе на хаб)
// 1 байта - адрес устройства на которое ADS1252 ответит пакетом RET_ADS1252_N_MEASURE
#define CMD_ADS1252_SET_ECHO_ADDR		0x25
	// сразу ответит RET_ADS1252_OK

// Команда запустить непрерывные измерение по N измерений
// 1 байт количество единичных изменений в отдоваемой сумме
#define CMD_ADS1252_START_INCESSANT		0x26
	// сразу ответит
	//#define	RET_ADS1252_OK

// Команда получить N измерений
#define CMD_ADS1252_GET_BUFFER			0x27
	// сразу ответит
	#define RET_ADS1252_BUFFER				0x24
	// 1 байт количество в буфуре измерений, N long измерений.

//Комманды для АЦП работающего в режиме модуляции

//В этой команде надо задать кол-во измерений за период
//Параметр 4 байт - кол-во периодов измерений
#define CMD_ADS1252_COUNT_MES_PERIOD   0x26
	#define RET_ADS1252_COUNT_MES_PERIOD   0x26
	// в параметрах float сумма N измерений, word N.

//Эта команда позволяет дпоуклчить среднее за период
//Параметр 4 байт - кол-во периодов измерений
#define CMD_ADS1252_AVERAGE_MES_PERIOD 0x27
	#define RET_ADS1252_AVERAGE_MES_PERIOD 0x27
	// в параметрах float сумма N измерений, word N.

//Эта команда позволяет работать в режиме синхронизирующего детектора
//Параметр 2 байт - кол-во периодов измерений
//Параметр 1 байт - сдвиг фазы
//Параметр 1 байт - накопление на полке
#define CMD_ADS1252_SYNCHRO_DETECTOR   0x28
	#define RET_ADS1252_SYNCHRO_DETECTOR   0x28
	// в параметрах float сумма N измерений, word N.

// Если на ADS1252 приходит пакет не от хаба, ADS1252 дабавит к нему
// RET_ADS1252_N_MEASURE и пошлёт на установленный адрес ECHO
// если адрес ECHO не установлен, то это адрес HUB_ADDR

//***********************************************************************************************
// Команды для АЦП на основе ADS1252  для инфракрасного датцика и модулятора на 200-400Гц
//***********************************************************************************************
// Размер блока данных при считывание кадра. 1 измерения это 3 байта (24 разрядное АЦП)
#define LONG_BLOCK_MEAS400	16
// Размер кадра. 1 измерения это 3 байта (24 разрядное АЦП) (15 блоков)
#define MAX_FRAME_MEAS400	256
// Запросить блок данных кадра...
// 1 байта - номер блока
#define CMD_IK400_ADC_GET_BLOCK			0x10
// Ответ
	#define RET_IK400_ADC_BLOCK				0x10
	// и LONG_BLOCK_MEAS400 * 3 байт данных измерений

// Запросить 1 ворота
#define CMD_IK400_ADC_GET_GATES1			0x11
// Ответ
	#define RET_IK400_ADC_GATES1				0x11
	// и BYTE начало и BYTE конец измерений. В номерах измерений
// Задать 1 ворота
//BYTE начало и BYTE конец измерений. В номерах измерений
#define CMD_IK400_ADC_SET_GATES1			0x12
// Ответ
	#define RET_IK400_ADC_OK					0x12

// Запросить 2 ворота
#define CMD_IK400_ADC_GET_GATES2			0x13
// Ответ
	#define RET_IK400_ADC_GATES2				0x13
	// и BYTE начало и BYTE конец измерений. В номерах измерений
// Задать 2 ворота
//BYTE начало и BYTE конец измерений. В номерах измерений
#define CMD_IK400_ADC_SET_GATES2			0x14
 //Ответ RET_S256_ADC_OK

// Запросить N циклов измерений
#define CMD_IK400_ADC_GET_N_LOOP			0x17
// Ответ
	#define RET_IK400_ADC_N_LOOP				0x15
	// и WORD циклов измерений
// Задать N циклов измерений
//WORD циклов измерений
#define CMD_IK400_ADC_SET_N_LOOP			0x18
// Ответ RET_S256_ADC_OK

// Произвести измерения
#define CMD_IK400_ADC_GET_MEASURE 	    	0x19
// Ответ
	#define RET_IK400_ADC_MEASURE				0x16
	// unsigned INT64 результирующая сумма по первым воротам
	// unsigned INT64 результирующая сумма по вторым воротам
	// char Statur - 0х01 ошибка модулятора, 0х02 ошибка зашкала, 0х04 ошибка ворот калибровки

// Начать измерения
#define CMD_IK400_ADC_START_MEASURE 		0x1A
// Ответ RET_S256_ADC_OK
// Ответ между командами, приложенний к пакету от мотора
	#define RET_IK400_ADC_N_MEASURE				0x17
	// unsigned long	количество полных циклов модулятора
	// unsigned INT64 результирующая сумма по первым воротам
	// unsigned INT64 результирующая сумма по вторым воротам
	// char Statur 0х01 ошибка модулятора, 0х02 ошибка зашкала, 0х04 ошибка ворот калибровки

// Остановить измерени
#define CMD_IK400_ADC_STOP_MEASURE 	    	0x1C
// Ответ RET_S256_ADC_OK

// Заморозить измерения
#define CMD_IK400_ADC_SLEEP_MEASURE 		0x1D
// Ответ RET_S256_ADC_OK

// Запустить измерения
#define CMD_IK400_ADC_GO_MEASURE 		   0x1E
// Ответ RET_S256_ADC_OK

	// ответ если была ошибка
	#define RET_IK400_ADC_ERROR					0x18
	// BYTE код ошибки
	#define CODE_ERROR_IK400_ADC_NO_MODULE			    0x01		//ошибка отсутствие сигнала модулятора
	#define CODE_ERROR_IK400_ADC_NO_GATES_MODULE		0x02		//ошибка период модулятора вышел из ворок при калибровки
	#define CODE_ERROR_IK400_ADC_ERROR_SCALE			0x03		//ошибка зашкал сигнала

// Запросить период модулятора
#define CMD_IK400_ADC_GET_MODULE			0x1F
// Ответ
	#define RET_IK400_ADC_MODULE					0x19
	// WORD текущий период модулятора
	// WORD минимальный период модулятора, команда сбрасывает значения
	// WORD максимальный период модулятора, команда сбрасывает значения
	// WORD минимальный период модулятора запомненный при записи ворот
	// WORD максимальный период модулятора запомненный при записи ворот


//***********************************************
#endif
//***********************************************
