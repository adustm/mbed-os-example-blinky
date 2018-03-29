#include "mbed.h"
#include "cmsis_os.h"
#include "PinNames.h"
#include "QSPI.h"

// define what flash we got
#define MX25R6435F

#if defined(MX25R6435F)
// Command for reading status register
#define QSPI_STD_CMD_RDSR                   0x05
// Command for writing status register
#define QSPI_STD_CMD_WRSR                   0x01
// Command for reading control register (supported only by some memories)
#define QSPI_STD_CMD_RDCR                   0x15
// Command for writing control register (supported only by some memories)
#define QSPI_STD_CMD_WRCR                   0x3E
// Command for setting Reset Enable (supported only by some memories)
#define QSPI_STD_CMD_RSTEN                  0x66
// Command for setting Reset (supported only by some memories)
#define QSPI_STD_CMD_RST                    0x99
// Command for setting WREN (supported only by some memories)
#define QSPI_STD_CMD_WREN                   0x06
// Command for Sector erase (supported only by some memories)
#define QSPI_STD_CMD_SECT_ERASE             0x20
// Command for writing (page programming)
#define QSPI_PAGE_PROG_CMD                  0x02
// Simple reading
#define QSPI_SIMPLE_READ_CMD                0x03
// Fast reading
#define QSPI_FAST_READ_CMD                  0x0B
/* Configuration Register 2 */
#define MX25R6435F_CR2_LH_SWITCH            ((uint8_t)0x02)    /*!< Low power / high performance switch */
#define MX25R6435F_PAGE_SIZE                 0x100     /* 32768 pages of 256 bytes */

#elif defined(N25Q128A)

// almost identical than MX25R6435F, just that sector erase is above for subsector erase?

// Command for reading status register
#define QSPI_STD_CMD_RDSR                   0x05
// Command for writing status register
#define QSPI_STD_CMD_WRSR                   0x01
// Command for reading control register (supported only by some memories)
#define QSPI_STD_CMD_RDCR                   0x35
// Command for writing control register (supported only by some memories)
#define QSPI_STD_CMD_WRCR                   0x3E
// Command for setting Reset Enable (supported only by some memories)
#define QSPI_STD_CMD_RSTEN                  0x66
// Command for setting Reset (supported only by some memories)
#define QSPI_STD_CMD_RST                    0x99
// Command for setting WREN (supported only by some memories)
#define QSPI_STD_CMD_WREN                   0x06
// Command for Sector erase (supported only by some memories)
#define QSPI_STD_CMD_SECT_ERASE             0x20
#endif

#if defined (TARGET_NORDIC)
// Read/Write commands
#define QSPI_PP_COMMAND_NRF_ENUM            (0x0) //This corresponds to Flash command 0x02
#define QSPI_READ2O_COMMAND_NRF_ENUM        (0x1) //This corresponds to Flash command 0x3B
#define QSPI_READ2IO_COMMAND_NRF_ENUM       (0x2) //This corresponds to Flash command 0xBB
#define QSPI_PP4IO_COMMAND_NRF_ENUM         (0x3) //This corresponds to Flash command 0x38
#define QSPI_READ4IO_COMMAND_NRF_ENUM       (0x4) //This corresponds to Flash command 0xEB

#else

 // use generic values

#define QSPI_PP_COMMAND_NRF_ENUM            (0x02)
#define QSPI_READ2O_COMMAND_NRF_ENUM        (0x3B)
#define QSPI_READ2IO_COMMAND_NRF_ENUM       (0xBB)
#define QSPI_PP4IO_COMMAND_NRF_ENUM         (0x38) // TODO: cant find this in the datasheet
#define QSPI_READ4IO_COMMAND_NRF_ENUM       (0xEB)

#endif


#if defined(TARGET_DISCO_F469NI)
#define QSPI_PIN_IO0 PF_8
#define QSPI_PIN_IO1 PF_9
#define QSPI_PIN_IO2 PF_7
#define QSPI_PIN_IO3 PF_6
#define QSPI_PIN_SCK PF_10
#define QSPI_PIN_CSN PB_6

#elif defined(TARGET_DISCO_L475VG_IOT01A)

#define QSPI_PIN_IO0 PE_12
#define QSPI_PIN_IO1 PE_13
#define QSPI_PIN_IO2 PE_14
#define QSPI_PIN_IO3 PE_15
#define QSPI_PIN_SCK PE_10
#define QSPI_PIN_CSN PE_11

#endif

#define DEBUG_ON 1
#ifdef DEBUG_ON
    #define VERBOSE_PRINT(x) printf x
#else    
    #define VERBOSE_PRINT(x)
#endif

#define DO_TEST( test )                                 \
    {                                                   \
        printf("\nExecuting test: %-40s :", #test );    \
        if( false == test() ) {                         \
            printf(" FAILED" );                         \
        } else {                                        \
            printf(" PASSED" );                         \
        }                                               \
    }                                                   \

QSPI *myQspi = NULL;
QSPI *myQspiOther = NULL;
#define _1_K_ (0x400)
#define _4_K_ (_1_K_ * 4)
    
bool InitializeFlashMem();
bool WaitForMemReady();
bool WriteEnable();
bool QSPI_HighPerfMode();
bool mx25r6435f_write(unsigned int flash_addr, const char *tx_buffer, size_t tx_length);
bool SectorErase(unsigned int flash_addr);
bool TestWriteReadSimple();
bool TestWriteReadBlockMultiplePattern();
bool TestWriteSingleReadMultiple();
bool TestWriteMultipleReadSingle();
bool TestWriteReadMultipleObjects();
bool TestWriteReadCustomCommands();
    
// main() runs in its own thread in the OS
int main() {
    myQspi = new QSPI((PinName)QSPI_PIN_IO0, (PinName)QSPI_PIN_IO1, (PinName)QSPI_PIN_IO2, (PinName)QSPI_PIN_IO3, (PinName)QSPI_PIN_SCK, (PinName)QSPI_PIN_CSN);        
    if(myQspi) {
        printf("\nCreated QSPI driver object succesfully");
    } else {
        printf("\nERROR: Failed creating QSPI driver object");
        return -1;
    }
    /* Change frequency to 20MHz*/
    if (QSPI_STATUS_OK != myQspi->set_frequency(20000000)) {
        VERBOSE_PRINT(("\nChange Frequency ERROR\n"));
        return -1;
    }

    ///////////////////////////////////////////
    // Run tests in QUADSPI 1_1_1 mode
    ///////////////////////////////////////////
    printf("\n\nQSPI Config = 1_1_1");
    if(QSPI_STATUS_OK == myQspi->configure_format( QSPI_CFG_BUS_SINGLE, QSPI_CFG_BUS_SINGLE, QSPI_CFG_ADDR_SIZE_24, QSPI_CFG_BUS_SINGLE, QSPI_CFG_ALT_SIZE_8, QSPI_CFG_BUS_SINGLE, 0, 0 )) {
        printf("\nConfigured QSPI driver configured succesfully");
    } else {
        printf("\nERROR: Failed configuring QSPI driver");
        return -1;
    }
    
    if( false == InitializeFlashMem()) {
        printf("\nUnable to initialize flash memory, tests failed\n");
        return -1;
    } else {
        printf("\nInitialize flash memory OK\n");
    }
    
    DO_TEST( TestWriteReadSimple );
    DO_TEST(TestWriteReadBlockMultiplePattern);
    DO_TEST(TestWriteMultipleReadSingle);
    DO_TEST(TestWriteSingleReadMultiple);
    if(NULL != myQspi)    
        delete myQspi;
    if(NULL != myQspiOther)
        delete myQspiOther;
    
    printf("\nDone...\n");
}

bool TestWriteReadSimple()
{
    printf("TestWriteReadSimple start\n");
    // TODO from the table for this flash, seems like 9 for fast read is supported
    //myQspi->set_frequency(9000000);
    int result = 0;
    char tx_buf[] = { 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x10, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x2F };    
    char rx_buf[16];    
    size_t buf_len = sizeof(tx_buf);
    
    uint32_t flash_addr = 0x100;
    if( false == SectorErase(flash_addr)) {
        printf("\nERROR: SectorErase failed(addr = 0x%08X)\n", (unsigned int)flash_addr);
        return false;
    }
    
    if( false == WaitForMemReady()) {
        printf("\nERROR: Device not ready, tests failed\n");
        return false;
    }

    //Send WREN
    if (!WriteEnable()) {
        return false;
    }

    if( !WaitForMemReady()) {
        printf("\nERROR: Device not ready, tests failed\n");
        return false;
    }

    printf(">>>>>>>>>>>>>>>>>>>>START WRITING...\n");
    /* bit mask last 8 bits of the adress */
    result = myQspi->write(QSPI_PAGE_PROG_CMD, -1, 0, (flash_addr & 0x00FFFF00), tx_buf, &buf_len );
    if( ( result != QSPI_STATUS_OK ) || buf_len != sizeof(tx_buf) ) {
        printf("\nERROR: Write failed. result = %d, bu_len= %lu", result, (uint32_t)buf_len);
    }

    if( false == WaitForMemReady()) {
        printf("\nERROR: Device not ready, tests failed\n");
        return false;
    }
    
    memset( rx_buf, 0, sizeof(rx_buf) );
    printf(">>>>>>>>>>>>>>>>>>>>START READING...\n");
    /* TEST IS OK WITH THIS READ FUNCTION :     */
    result = myQspi->read(QSPI_FAST_READ_CMD, -1, 8, flash_addr, rx_buf, &buf_len );
    /* TEST IS also OK WITH THIS READ FUNCTION :     */
    //result = myQspi->read(QSPI_SIMPLE_READ_CMD, -1, 0, flash_addr, rx_buf, &buf_len );
    if( result != QSPI_STATUS_OK ) {
        printf("\nERROR: Read failed");
        return false;
    }
    if( buf_len != sizeof(rx_buf) ) {
        printf( "\nERROR: Read %d values instead of %d\n", buf_len, sizeof(rx_buf));
        return false;
    }

    for (size_t i = 0; i<buf_len; i++) {
        if (rx_buf[i] != tx_buf[i]) {
            printf("\nERROR: Buffer[%d] content invalid: sent %02X, received %02X", i, tx_buf[i], rx_buf[i] );
            return false;
        }
    }
//    if(0 != (memcmp( rx_buf, tx_buf, sizeof(rx_buf)))) {
    
    return true;
}

bool InitializeFlashMem()
{
    bool ret_status = true;
    char status_value[2] = {0};
    printf("InitializeFlashMem:\n");
    //Read the Status Register from device
    if (QSPI_STATUS_OK == myQspi->command_transfer(QSPI_STD_CMD_RDSR, // command to send
                              -1,
                              NULL,              // do not transmit
                              0,                 // do not transmit
                              status_value,                 // just receive two bytes of data
                              1)) {   // store received values in status_value
        VERBOSE_PRINT(("\nReading Status Register Success: value = 0x%02X\n", status_value[0]));
    } else {
        printf("\nERROR: Reading Status Register failed\n");
        ret_status = false;
    }
    
    if(ret_status)
    {
        //Send Reset Enable
        if (QSPI_STATUS_OK == myQspi->command_transfer(QSPI_STD_CMD_RSTEN, // command to send
                                  -1,
                                  NULL,              // do not transmit
                                  0,                 // do not transmit
                                  NULL,                 // just receive two bytes of data
                                  0)) {   // store received values in status_value
            VERBOSE_PRINT(("\nSending RSTEN Success\n"));
        } else {
            printf("\nERROR: Sending RSTEN failed\n");
            ret_status = false;
        }
        

        if(ret_status)
        {
            //Send Reset
            if (QSPI_STATUS_OK == myQspi->command_transfer(QSPI_STD_CMD_RST, // command to send
                                      -1,
                                      NULL,           // do not transmit
                                      0,              // do not transmit
                                      NULL,           // just receive two bytes of data
                                      0)) {   // store received values in status_value
                VERBOSE_PRINT(("\nSending RST Success\n"));
            } else {
                printf("\nERROR: Sending RST failed\n");
                ret_status = false;
            }

            WaitForMemReady();
        }
    }
    /* Enable High Performance mode */
    if (!QSPI_HighPerfMode()) {
        VERBOSE_PRINT(("\nHigh performance setting ERROR\n"));
        return false;
    }
    /* 0x85 and 0x81 are  unknown commands for MX25R6435F */
#if !defined(MX25R6435F)
    status_value[0] = 0;
    status_value[1] = 0;

    if(ret_status) {
        //read VCR
        if (QSPI_STATUS_OK == myQspi->command_transfer(0x85, // command to send
                                  -1,
                                  NULL,              // do not transmit
                                  0,                 // do not transmit
                                  &status_value[0],                 // just receive two bytes of data
                                  1)) {   // store received values in status_value
            VERBOSE_PRINT(("\nReceiving VCR Success\n"));
            printf("Read VCR: %d \n", status_value[0]);
        } else {
            printf("\nERROR: Receiving VCR failed\n");
            ret_status = false;
        }

        if(ret_status) {
            status_value[0] &= 0x0F; // set to 0 first
            status_value[0] |= 8 << 4; // 7:4 in VCR

            if (QSPI_STATUS_OK == myQspi->command_transfer(0x81, // command to send
                                        -1,
                                      &status_value[0],                 
                                      1,      
                                      NULL,                 
                                      0)) {   // store received values in status_value
                VERBOSE_PRINT(("\nWriting VCR dummy cycle Success\n"));
            } else {
                printf("\nERROR: Writing VCR dummy cycle failed\n");
                ret_status = false;
            }
        }

        if (ret_status) {
            //read VCR
            if (QSPI_STATUS_OK == myQspi->command_transfer(0x85, // command to send
                                      -1,
                                      NULL,              // do not transmit
                                      0,                 // do not transmit
                                      &status_value[0],                 // just receive two bytes of data
                                      1)) {   // store received values in status_value
                if (((uint8_t)status_value[0] >> 4) != 0x8) { 
                    VERBOSE_PRINT(("\n Failed to write VCR\n"));
                    printf("VCR: %d \n", status_value[0]);
                } else {
                    VERBOSE_PRINT(("\nReceiving VCR Success\n"));
                }
            } else {
                printf("\nERROR: Receiving VCR failed\n");
                ret_status = false;
            }
            
        }
    }
#endif
    return ret_status;
}

bool WaitForMemReady()
{
    char status_value[2];
    int retries = 0;

    do
    {
        retries++;
        //Read the Status Register from device
        if (QSPI_STATUS_OK == myQspi->command_transfer(QSPI_STD_CMD_RDSR, // command to send
                                  -1,
                                  NULL,                 // do not transmit
                                  0,              // do not transmit
                                  status_value,                 // just receive two bytes of data
                                  2)) {   // store received values in status_value
            VERBOSE_PRINT(("\nReadng Status Register Success: value = 0x%02X:0x%02X\n", status_value[0], status_value[1]));
        } else {
            printf("\nERROR: Reading Status Register failed\n");
        }
    } while( (status_value[0] & 0x1) != 0 && retries <10000 );
    
    if((status_value[0] & 0x1) != 0) {
        printf("WaitforMemReady FALSE\n");
        return false;
    }
    printf("WaitforMemReady TRUE\n");
    return true;
}

bool SectorErase(unsigned int flash_addr)
{
    printf("Sector Erase start \n");

    //Send WREN
    if (!WriteEnable()) {
        return false;
    }

    if( false == WaitForMemReady()) {
        VERBOSE_PRINT(("\nERROR: Device not ready, tests failed\n"));
        return false;
    }

    if (QSPI_STATUS_OK == myQspi->command_transfer(QSPI_STD_CMD_SECT_ERASE, // command to send
                              (flash_addr & 0x00FFF000),
                              NULL,                 // do not transmit
                              0,              // do not transmit
                              NULL,                 // just receive two bytes of data
                              0)) {   // store received values in status_value
        VERBOSE_PRINT(("\nSending SECT_ERASE command success\n"));
    } else {
        VERBOSE_PRINT(("\nERROR: Sending SECT_ERASE command failed\n"));
        return false;
    }
    
    if( false == WaitForMemReady()) {
        VERBOSE_PRINT(("\nERROR: Device not ready, tests failed\n"));
        return false;
    }
    VERBOSE_PRINT(("Sector Erase OK\n"));

    return true;
}

bool WriteEnable()
{

    if (QSPI_STATUS_OK != myQspi->command_transfer(QSPI_STD_CMD_WREN, // command to send
                              -1,
                              NULL,              // do not transmit
                              0,                 // do not transmit
                              NULL,              // just receive two bytes of data
                              0)) {
        VERBOSE_PRINT(("\nERROR:Sending WREN command FAILED\n"));
        return false;
    }
    printf("\n end of write enable OK\n");
    return true;
}

/**
  * @brief  This function enables/disables the high performance mode of the memory.
  * @param  hqspi     : QSPI handle
  * @param  Operation : QSPI_HIGH_PERF_ENABLE or QSPI_HIGH_PERF_DISABLE high performance mode    
  * @retval None
  */
bool QSPI_HighPerfMode() {
    char reg[3] = {0};
    VERBOSE_PRINT(("\n>>>>>>>>>>>>>>>>>>>>START SETTING HIGH PERF...\n"));
    /* Read Status byte */
    if (QSPI_STATUS_OK != myQspi->command_transfer(QSPI_STD_CMD_RDSR, // command to send
                            -1,            // no address
                            NULL, 0,       // do not transmit
                            reg, 1)) {     //  receive 1 bytes of data
        VERBOSE_PRINT(("\nERROR: Reading Status Register failed\n"));
        return false;
    }
    WaitForMemReady();
    /* Read Configuration bytes */
    if (QSPI_STATUS_OK != myQspi->command_transfer(QSPI_STD_CMD_RDCR, // command to send
                                  -1,            // no address
                                  NULL, 0,       // do not transmit
                                  (reg+1), 2)) { //receive two bytes of data
        VERBOSE_PRINT(("\nERROR: Reading Configuration Registers failed\n"));
        return false;

    }
    WaitForMemReady();
    if ((reg[2] & MX25R6435F_CR2_LH_SWITCH) != MX25R6435F_CR2_LH_SWITCH) {
        VERBOSE_PRINT(("\n>>>>>>>>>>>>>>>>>>>>need to write HIGH PERF...\n"));
        reg[2] |= MX25R6435F_CR2_LH_SWITCH;
        /* Enable write operations */
        if (!WriteEnable()) {
            VERBOSE_PRINT(("\nERROR in write enable \n"));
            return false;
        }
        if (QSPI_STATUS_OK != myQspi->command_transfer(QSPI_STD_CMD_WRSR, // command to send
                                -1,           // no address
                                reg, 3,       // transmit status+config regs (3 bytes)
                                NULL, 0)) {   //  do not receive
            VERBOSE_PRINT(("\nERROR: Reading Status Register failed\n"));
            return false;
        }
        WaitForMemReady();
        /* Readback the configuration register to check */
        reg[1] = 0; reg[2] = 0;
        /* Read Configuration bytes */
        if (QSPI_STATUS_OK != myQspi->command_transfer(QSPI_STD_CMD_RDCR, // command to send
                                      -1,            // no address
                                      NULL, 0,       // do not transmit
                                      &reg[1], 2)) { //receive two bytes of data
            VERBOSE_PRINT(("\nERROR: Reading Configuration Registers failed\n"));
            return false;
        }
        WaitForMemReady();
        if ((reg[2] & MX25R6435F_CR2_LH_SWITCH) != MX25R6435F_CR2_LH_SWITCH) {
            return false;
        }
    }
    VERBOSE_PRINT(("\n>>>>>>>>>>>>>>>>>>>>END SETTING HIGH PERF..."));
    return true;

}

bool mx25r6435f_write(unsigned int flash_addr, const char *tx_buffer, size_t tx_length)
{
    size_t end_addr, current_size, current_addr;
    int result = 0;
    printf(">>>>> START mx25r6435f_write at 0X%08X (size %d) \n", flash_addr, tx_length);
    /* Calculation of the size between the write address and the end of the page */
    current_size = MX25R6435F_PAGE_SIZE - (flash_addr % MX25R6435F_PAGE_SIZE);

    /* Check if the size of the data is less than the remaining place in the page */
    if (current_size > tx_length) {
        current_size = tx_length;
    }

    /* Initialize the adress variables */
    current_addr = flash_addr;
    end_addr = flash_addr + tx_length;
    /* Perform the write page by page */
    do {
        //Send WREN
        if (!WriteEnable()) {
            return false;
        }

        if (!WaitForMemReady()) {
            printf("\nERROR: Device not ready, tests failed\n");
            return false;
        }

        result = myQspi->write(QSPI_PAGE_PROG_CMD, -1, 0, (current_addr & 0x00FFFF00), tx_buffer, &current_size );
        if (result != QSPI_STATUS_OK) {
            printf("\nERROR: Write failed. Result=%d, Current_size=%d\n", result, current_size);
            return false;
        } else {
            printf("\n Write OK\n");
        }

        if (!WaitForMemReady()) {
            printf("\nERROR: Device not ready, tests failed\n");
            return false;
        }

        /* Update the address and size variables for next page programming */
        current_addr += current_size;
        tx_buffer += current_size;
        current_size = ((current_addr + MX25R6435F_PAGE_SIZE) > end_addr) ? (end_addr - current_addr) : MX25R6435F_PAGE_SIZE;
    } while (current_addr < end_addr);

    printf(">>>>> END   mx25r6435f_write at 0X%08X (size %d) \n", flash_addr, tx_length);
    return true;
}

bool TestWriteReadBlockMultiplePattern()
{
    char *test_tx_buf = NULL;
    char *test_rx_buf = NULL;
    char *test_tx_buf_aligned = NULL;
    uint32_t flash_addr = 0;
    int result = 0;
    size_t buf_len = 0;
    char pattern_buf[] = { 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x10, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x2F };
    printf("\n********************************************************\nTestWriteReadBlockMultiplePattern start\n********************************************************\n");

    test_tx_buf = NULL;
    test_tx_buf = (char *)malloc( _1_K_ * 2 ); //Alloc 2k to get a 1K boundary
    if(test_tx_buf == NULL) {
        printf("\nERROR: tx buf alloc failed");
        return false;
    }
    test_tx_buf_aligned = (char *)((((uint32_t)test_tx_buf) + _1_K_) & 0xFFFFFC00);

    test_rx_buf = NULL;
    test_rx_buf = (char *)malloc( _1_K_ ); //Alloc 2k to get a 1K boundary
    if(test_rx_buf == NULL) {
        printf("\nERROR: rx buf alloc failed");
        return false;
    }

    flash_addr = 0x2000;
    for(int i=0; i < 16; i++) {
        printf(">>>>>>>>>>FOR LOOP ... %d of 15\n", i);
        if( false == SectorErase(flash_addr)) {
            printf("\nERROR: SectorErase failed(addr = 0x%08X)\n", flash_addr);
            return false;
        }

        if( false == WaitForMemReady()) {
            printf("\nDevice not ready, tests failed\n");
            return false;
        }

        memset( test_tx_buf_aligned, pattern_buf[i], _1_K_ );
        buf_len = _1_K_; //1k
        result = mx25r6435f_write( flash_addr, test_tx_buf_aligned, buf_len );
        if( ( result != true ) || buf_len != _1_K_ ) {
            printf("\nERROR: Write failed");
            return false;
        } else { printf("\n Write OK\n");
        }

        if( false == WaitForMemReady()) {
            printf("\nERROR: Device not ready, tests failed\n");
            return false;
        }

        memset( test_rx_buf, 0, _1_K_ );
        buf_len = _1_K_; //1k
        result = myQspi->read(QSPI_FAST_READ_CMD, -1, 8, flash_addr, test_rx_buf, &buf_len );
        if( result != QSPI_STATUS_OK ) {
            printf("\nERROR: Read failed");
            return false;
        } else { printf("\n read OK\n");
        }
        if( buf_len != _1_K_ ) {
            printf( "\nERROR: Unable to read the entire buffer" );
            return false;
        } else {
            printf("Size read ok\n");
        }

        for (size_t i = 0; i<_1_K_; i++) {
            if (test_rx_buf[i] != test_tx_buf_aligned[i]) {
                printf("\nERROR: Buffer[%d] content invalid: sent %02X, received %02X", i, test_tx_buf_aligned[i], test_rx_buf[i] );
                return false;
            }
        }
        printf("test_rx_buf content OK \n");

        flash_addr += 0x1000;
    }

    free(test_rx_buf);
    free(test_tx_buf);

    return true;
}

bool TestWriteMultipleReadSingle()
{
    char *test_tx_buf = NULL;
    char *test_rx_buf = NULL;
    char *test_tx_buf_aligned = NULL;
    char *test_rx_buf_aligned = NULL;
    char *tmp = NULL;
    uint32_t flash_addr = 0;
    int result = 0;
    size_t buf_len = 0;
    char pattern_buf[] = { 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x10, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x2F };  
    unsigned int start_addr = 0x2000;
    printf("\n********************************************************\nTestWriteMultipleReadSingle start\n********************************************************\n");

    test_tx_buf = NULL;
    test_tx_buf = (char *)malloc( _1_K_ * 5 ); //Alloc 5k to get a 1K boundary
    if(test_tx_buf == NULL) {
        printf("\nERROR: tx buf alloc failed");
        return false;
    }
    test_tx_buf_aligned = (char *)((((uint32_t)test_tx_buf) + _1_K_) & 0xFFFFFC00);

    test_rx_buf = NULL;
    test_rx_buf = (char *)malloc( _1_K_ * 5 ); //Alloc 5k to get a 1K boundary
    if(test_rx_buf == NULL) {
        printf("\nERROR: rx buf alloc failed");
        return false;
    }
    test_rx_buf_aligned = (char *)((((uint32_t)test_rx_buf) + _1_K_) & 0xFFFFFC00);

    flash_addr = start_addr;
    /* It's ok to erase only 1 sector as start_addr=0x2000 is aligned with a 4K sector start */
    if( false == SectorErase(flash_addr)) {
        printf("\nERROR: SectorErase failed(addr = 0x%08X)\n", flash_addr);
        return false;
    }

    if( false == WaitForMemReady()) {
        printf("\nDevice not ready, tests failed\n");
        return false;
    }

    /* Handle several 1 K write */
    tmp = test_tx_buf_aligned;
    for( int i=0; i < 4; i++) {
        printf(">>>>>>>>>>FOR LOOP ... %d of 4\n", i);
        memset( tmp, pattern_buf[i], _1_K_ );
        buf_len = _1_K_; //1k
        result = mx25r6435f_write( flash_addr, tmp, buf_len );
        if( ( result != true ) || buf_len != _1_K_ ) {
            printf("\nERROR: Write failed");
            return false;
        }

        if( false == WaitForMemReady()) {
            printf("\nERROR: Device not ready, tests failed\n");
            return false;
        }
        flash_addr += _1_K_;
        tmp += _1_K_;
    }

    /* Handle 4K read in a single call */
    memset( test_rx_buf_aligned, 0, _4_K_ );
    buf_len = _4_K_; //4 * 1k
    flash_addr = start_addr;
    result = myQspi->read(QSPI_FAST_READ_CMD, -1, 8, flash_addr, test_rx_buf_aligned, &buf_len );
    if( result != QSPI_STATUS_OK ) {
        printf("\nERROR: Read failed");
        return false;
    }
    if( buf_len != _4_K_ ) {
        printf( "\nERROR: Unable to read the entire buffer" );
        return false;
    }
    if(0 != (memcmp( test_rx_buf_aligned, test_tx_buf_aligned, _4_K_))) {
        printf("\nERROR: Buffer contents are invalid");
        return false;
    }

    free(test_rx_buf);
    free(test_tx_buf);

    return true;
}

bool TestWriteSingleReadMultiple()
{
    char *test_tx_buf = NULL;
    char *test_rx_buf = NULL;
    char *test_tx_buf_aligned = NULL;
    char *test_rx_buf_aligned = NULL;
    char *tmp = NULL;
    uint32_t flash_addr = 0;
    int result = 0;
    size_t buf_len = 0;
    char pattern_buf[] = { 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x10, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x2F };
    unsigned int start_addr = 0x2000;
    printf("\n********************************************************\nTestWriteSingleReadMultiple start\n********************************************************\n");

    test_tx_buf = NULL;
    test_tx_buf = (char *)malloc( _1_K_ * 5 ); //Alloc 5k to get a 1K boundary
    if(test_tx_buf == NULL) {
        printf("\nERROR: tx buf alloc failed");
        return false;
    }
    test_tx_buf_aligned = (char *)((((uint32_t)test_tx_buf) + _1_K_) & 0xFFFFFC00);

    test_rx_buf = NULL;
    test_rx_buf = (char *)malloc( _1_K_ * 5 ); //Alloc 5k to get a 1K boundary
    if(test_rx_buf == NULL) {
        printf("\nERROR: rx buf alloc failed");
        return false;
    }
    test_rx_buf_aligned = (char *)((((uint32_t)test_rx_buf) + _1_K_) & 0xFFFFFC00);

    /* Need to Erase 4 sectors of 1K each */
    flash_addr = start_addr;
    for (int i= 0; i<4; i++) {
        if( false == SectorErase(flash_addr)) {
            printf("\nERROR: SectorErase failed(addr = 0x%08X)\n", flash_addr);
            return false;
        }

        if( false == WaitForMemReady()) {
            printf("\nDevice not ready, tests failed\n");
            return false;
        }
        flash_addr += _1_K_;
    }

    flash_addr = start_addr;

    tmp = test_tx_buf_aligned;
    for( int i=0; i < 4; i++) {
        memset( tmp, pattern_buf[i], _1_K_ );
        tmp += _1_K_;
    }

    buf_len = _4_K_; //4k
    result = mx25r6435f_write( flash_addr, test_tx_buf_aligned, buf_len );
    if( ( result != true ) || buf_len != _4_K_ ) {
        printf("\nERROR: Write failed");
        return false;
    }

    if( false == WaitForMemReady()) {
        printf("\nERROR: Device not ready, tests failed\n");
        return false;
    }

    memset( test_rx_buf_aligned, 0, _4_K_ );

    buf_len = _1_K_; //1k
    flash_addr = start_addr;
    tmp = test_rx_buf_aligned;
    for( int i=0; i < 4; i++) {
        result = myQspi->read(QSPI_FAST_READ_CMD, -1, 8, flash_addr, tmp, &buf_len );
        if( result != QSPI_STATUS_OK ) {
            printf("\nERROR: Read failed");
            return false;
        }
        if( buf_len != _1_K_ ) {
            printf( "\nERROR: Unable to read the entire buffer" );
            return false;
        }
        tmp += _1_K_;
        flash_addr += _1_K_;
    }
    if(0 != (memcmp( test_rx_buf_aligned, test_tx_buf_aligned, _4_K_))) {
        printf("\nERROR: Buffer contents are invalid");
        return false;
    }

    free(test_rx_buf);
    free(test_tx_buf);

    return true;
}

