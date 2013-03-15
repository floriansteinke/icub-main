// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/*
* Copyright (C) 2012 Robotcub Consortium
* Author: Alberto Cardellino
* CopyPolicy: Released under the terms of the GNU GPL v2.0.
*
*/

/// general purpose stuff.

#include <yarp/os/Time.h>
#include <stdarg.h>
#include <stdio.h>
#include <yarp/dev/PolyDriver.h>
#include <ace/config.h>
#include <ace/Log_Msg.h>

// #include "ThreadTable2.h"
// #include "ThreadPool2.h"

#include <string>
#include <iostream>
#include <string.h>


/// specific to this device driver.
#include "embObjAnalogSensor.h"

// #include <yarp/dev/ControlBoardInterfacesImpl.inl>

#ifdef WIN32
#pragma warning(once:4355)
#endif

const int REPORT_PERIOD=6; //seconds
const double BCAST_STATUS_TIMEOUT=6; //seconds


using namespace yarp;
using namespace yarp::os;
using namespace yarp::dev;

inline bool NOT_YET_IMPLEMENTED(const char *txt)
{
    yWarning() << std::string(txt) << " not yet implemented for embObjAnalogSensor\n";
    return false;
}

//generic function that check is key1 is present in input bottle and that the result has size elements
// return true/false
static inline bool validate(Bottle &input, Bottle &out, const std::string &key1, const std::string &txt, int size)
{
    size++;  // size includes also the name of the parameter
    Bottle &tmp=input.findGroup(key1.c_str(), txt.c_str());
    if (tmp.isNull())
    {
        fprintf(stderr, "%s not found\n", key1.c_str());
        return false;
    }

    if(tmp.size()!=size)
    {
        fprintf(stderr, "%s incorrect number of entries\n", key1.c_str());
        return false;
    }

    out=tmp;

    return true;
}

bool embObjAnalogSensor::fromConfig(yarp::os::Searchable &_config)
{
    Bottle xtmp;

 
  // embObj parameters, in ETH group
    Value val =_config.findGroup("ETH").check("Ems",Value(1), "Board number");
    if(val.isInt())
        _fId.boardNum =val.asInt();
    else
    {
        yError () << "embObjAnalogSensor: EMS Board number identifier not found\n";
        return false;
    }

    // Analog Sensor stuff
    Bottle config = _config.findGroup("GENERAL");
    if (!validate(config, xtmp, "Period","transmetting period of the sensor", 1))
    {
        yError() << "embObjAnalogSensor Using default value = 0 (disabled)";
        _period = 0;
    }
    else
    {
        _period = xtmp.get(1).asInt();
        yDebug() << "embObjAnalogSensor Using value of" << _period;
    }

    if (!validate(config, xtmp, "Channels","Number of channels of the Analog Sensor", 1))
    {
        fprintf(stderr, "embObjAnalogSensor: Using default value = 0 (disabled)\n");
        _channels = 0;
        _period   = 0;
    }
    else
    {
        _channels = xtmp.get(1).asInt();
    }

    if (!validate(config, xtmp, "UseCalibration","Calibration parameters are needed", 1))
    {
        fprintf(stderr, "embObjAnalogSensor: Using default value = 0 (Don't use calibration)\n");
        _useCalibration = 0;
    }
    else
    {
        _useCalibration = xtmp.get(1).asInt();
    }
};


embObjAnalogSensor::embObjAnalogSensor(): data(0)
{
    _useCalibration=0;
    _channels=0;
    _period=0;

    scaleFactor=0;

    timeStamp=0;
    counterSat=0;
    counterError=0;
    counterTimeout=0;

    status=IAnalogSensor::AS_OK;
}

embObjAnalogSensor::~embObjAnalogSensor()
{
    if (!data)
        delete data;
    if (!scaleFactor)
        delete scaleFactor;
}

bool embObjAnalogSensor::open(yarp::os::Searchable &config)
{
    Property prop;
    std::string str=config.toString().c_str();
    yTrace() << str;

    // Set dummy values
    _fId.boardNum  = 255;
    _fId.ep = 255;

    fromConfig(config);
    prop.fromString(str.c_str());

    // get EMS ip address from config string
    ACE_TCHAR address[64] = {0};
    Bottle xtmp = Bottle(config.findGroup("ETH"));
    strcpy(address, config.findGroup("ETH").check("IpAddress",Value(1), "EMS ip address").asString().c_str() );

    //   Debug info
    memset(info, 0x00, SIZE_INFO);
    sprintf(info, "embObjAnalogSensor: referred to EMS: %d at address %s\n", _fId.boardNum, address);

    //   open ethResource, if needed
    ethResCreator *resList = ethResCreator::instance();
    if(NULL == (res = resList->getResource(config)) )
    {
        yError () << "embObjAnalogSensor: Unable to instantiate an EMS... check configuration file";
        return false;
    }

    switch(_fId.boardNum)
    {
    case 1:
        _fId.ep = endpoint_as_leftupperarm;
        break;
    case 2:
        _fId.ep = endpoint_as_leftlowerarm;
        break;
    case 3:
        _fId.ep = endpoint_as_rightupperarm;
        break;
    case 4:
        _fId.ep = endpoint_as_rightlowerarm;
        break;
    case 6:
        _fId.ep = endpoint_as_leftupperleg;
        break;
    case 8:
        _fId.ep = endpoint_as_rightupperleg;
        break;
    default:
        _fId.ep = 255;
        yError () << "\n embObjAnalogSensor: Found non-existing board identifier number!!!";
        return false;
        break;
    }

    data=new AnalogData(_channels, _channels+1);
    scaleFactor=new double[_channels];
    int i=0;
    for (i=0; i<_channels; i++) scaleFactor[i]=1;

    // Real values will be read from the sensor itself during its initalization hereafter
    for(int i=0; i<_channels; i++)
    {
        scaleFactor[i]=1;
    }

    // Tell EMS to go into config state, otherwise something doesn't work correctly.
#warning "go to config message before getting strain fullscale... investigate more
//     res->goToConfig();
    // Check inital size of array...  it should be zero.
    bool gotFullScaleValues = false;
    int timeout, NVsize;
    uint16_t tmpNVsize;
    EOnv tmpNV, *p_tmpNV;
    eOnvID_t nvid_strain_config, nvid_fullscale;
    eOsnsr_arrayofupto12bytes_t fullscale_values;
/*  can't do a reset if array is not correctly initialized with eo_new_blablabla function.
 * This needs knowing how data are actually stored, bytes dimension etc... which is quite low level knowledge to be placed here.
    I rely on the fact that iitialization of the variable is correctly done by transceiver.
     eo_array_New(&fullscale_values)
     eo_array_Reset((EOarray*) &fullscale_values);

     or better, just check that initalization has been done as expected, i.e. initial size is zero.
*/
    nvid_fullscale = eo_cfg_nvsEP_as_strain_NVID_Get((eOcfg_nvsEP_as_endpoint_t) _fId.ep, (eOcfg_nvsEP_as_strainNumber_t) 0, (eOcfg_nvsEP_as_strainNVindex_t) strainNVindex_sstatus__fullscale);
    if(EOK_uint16dummy == nvid_fullscale)
        yError() << "nvid not found";

    p_tmpNV = res->transceiver->getNVhandler( _fId.ep, nvid_fullscale, &tmpNV);

    // tmpNVsize is the actual dimension of the array expressed in bytes, it is NOT the number of elements the array contains
    res->transceiver->readValue( nvid_fullscale, _fId.ep, &fullscale_values, &tmpNVsize);

    yDebug() << "using pointer size     is " << eo_array_Size((EOarray*)     p_tmpNV->rem) << "or" << tmpNVsize;
    yDebug() << "using pointer capacity is " << eo_array_Capacity((EOarray*) p_tmpNV->rem);
    yDebug() << "using pointer itemsize is " << eo_array_ItemSize((EOarray*) p_tmpNV->rem);
 
    // when initialized, size shuold be zero... check it
    NVsize = eo_array_Size((EOarray*) p_tmpNV->rem);
    if(0 != NVsize)
        yError() << "Initial size of array is different from zero (" << NVsize << ")!!!";

     // Prepare analog sensor
    eOsnsr_strain_config_t strainConfig;
    strainConfig.datarate = _period;
    strainConfig.mode = snsr_strainmode_acquirebutdonttx;
    strainConfig.signaloncefullscale = eobool_true;

    nvid_strain_config = eo_cfg_nvsEP_as_strain_NVID_Get((eOcfg_nvsEP_as_endpoint_t) _fId.ep, (eOcfg_nvsEP_as_strainNumber_t) 0, (eOcfg_nvsEP_as_strainNVindex_t) strainNVindex_sconfig);
    res->transceiver->addSetMessage(nvid_strain_config, _fId.ep, (uint8_t *) &strainConfig);

    timeout = 4;
    // wait for rensponse
    if(_useCalibration)
    {
        while(!gotFullScaleValues && (timeout != 0) )
        {
            // read fullscale values
            res->transceiver->readValue( nvid_fullscale, _fId.ep, &fullscale_values, &tmpNVsize);
            // If data arrives, size is bigger than zero
            NVsize = eo_array_Size((EOarray*) p_tmpNV->rem);
            if(0 != NVsize)
            {
                gotFullScaleValues = true;
                break;
            }
            timeout--;
            printf("full scale val not arrived yet... retrying in 1 sec\n");
            Time::delay(1);
        }

        if (0 == timeout)
        {
            yError() << "ETH Analog sensor: request for calibration parameters timed out for board" << _fId.boardNum;
//         return false;  // commented for denug purpose... uncomment!!
        }

        if( (NVsize != _channels)  )
        {
            yError() << "Analog sensor Calibration data has a different size from channels number in configuration file!! Aborting! NVsize" << NVsize << "channels " << _channels;
            gotFullScaleValues = true;
//         return false;
        }

        uint8_t *msg;

        yError() << "capacity " << fullscale_values.head.capacity;
        yError() << "itemsize " << fullscale_values.head.itemsize;
        yError() << "size " << fullscale_values.head.size;

        if(gotFullScaleValues)
        {
            yWarning() << "GOT full scale values!! YUPPIE!!for board " << _fId.boardNum;

            for(int k=0; k<12; k++)
            	printf("%d(0x%0X) ", fullscale_values.data[k], fullscale_values.data[k]);
            printf("\n");

			for(int i=0; i<_channels; i++)
            {
                // Get the k-th element of the array as a 2 bytes msg
                msg = (uint8_t*) eo_array_At((EOarray*) &fullscale_values, i);


                // Got from CanBusMotionControl... here order of bytes seems inverted with respect to calibratedValues or uncalibratedValues (see callback)
                scaleFactor[i]= 0;
                scaleFactor[i]= ( (uint16_t)(msg[0]<<8) | msg[1] );
                yError() << " scale factor[" << i << "] = " << scaleFactor[i];
            }

        }
        strainConfig.mode = snsr_strainmode_txcalibrateddatacontinuously;
    }
    else
    {
        strainConfig.mode = snsr_strainmode_txuncalibrateddatacontinuously;
    }

    // Set analog sensor to start signalling data
    strainConfig.datarate = _period;
    strainConfig.signaloncefullscale = eobool_false;

//     nvid_strain_config = eo_cfg_nvsEP_as_mais_NVID_Get((eOcfg_nvsEP_as_endpoint_t) _fId.ep, (eOcfg_nvsEP_as_maisNumber_t) 0, (eOcfg_nvsEP_as_maisNVindex_t) strainNVindex_sconfig);
    res->transceiver->addSetMessage(nvid_strain_config, _fId.ep, (uint8_t *) &strainConfig);

    // Set variable to be signalled
    init();

    // Save eo data of this board/EP
    _fId.type=AnalogStrain;
    res->transceiver->getHostData(&_fId.EPvector, &_fId.EPhash_function);

    //N.B.: use a dynamic_cast to extract correct interface when using this pointer
    _fId.handle  = (this);

    memset(_fId.name, 0x00, SIZE_INFO);
    sprintf(_fId.name, "%s", info);       // Saving User Friendly Id
    resList->addLUTelement(_fId);
    res->goToRun();
    return true;
}

bool embObjAnalogSensor::init()
{
    yTrace();
    eOsnsr_strain_config_t strainConfig;
    eOnvID_t nvid;

    // Configure values to be sent regularly
    eOnvID_t                  nvid_ropsigcfgassign;       // nvID
    eOmn_ropsigcfg_command_t  *ropsigcfgassign;           // pointer to the type, to handle content
    EOnv                      nv_ropsigcfgassign;         // memory to hold stuff
    EOnv                      *nvRoot_ropsigcfgassign;    // pointer to the nv_ropsigcfgassign
    eOropSIGcfg_t             sigcfg;                     // struct for the single element to be signalled
    EOarray                   *array;                     // array containing nvids to be signalled

    nvid_ropsigcfgassign = eo_cfg_nvsEP_mn_comm_NVID_Get(endpoint_mn_comm, 0, commNVindex__ropsigcfgcommand);
    nvRoot_ropsigcfgassign = res->transceiver->getNVhandler(endpoint_mn_comm, nvid_ropsigcfgassign, &nv_ropsigcfgassign);

    ropsigcfgassign = (eOmn_ropsigcfg_command_t*) nvRoot_ropsigcfgassign->loc;
    array = (EOarray*) &ropsigcfgassign->array;
    eo_array_Reset(array);
    array->head.capacity = NUMOFROPSIGCFG;
    array->head.itemsize = sizeof(eOropSIGcfg_t);
    ropsigcfgassign->cmmnd = ropsigcfg_cmd_append;

    if(_useCalibration)
        nvid = eo_cfg_nvsEP_as_strain_NVID_Get((eOcfg_nvsEP_as_endpoint_t)_fId.ep, 0, (eOcfg_nvsEP_as_strainNVindex_t) strainNVindex_sstatus__calibratedvalues);
    else
        nvid = eo_cfg_nvsEP_as_strain_NVID_Get((eOcfg_nvsEP_as_endpoint_t)_fId.ep, 0, (eOcfg_nvsEP_as_strainNVindex_t) strainNVindex_sstatus__uncalibratedvalues);

    if(EOK_uint16dummy == nvid)
    {
        yError () << " EmbObj Analog Sensor NVID not found for EndPoint" << _fId.ep <<" at line " << __LINE__;
    }
    else
    {
        sigcfg.ep = _fId.ep;
        sigcfg.id = nvid;
        sigcfg.plustime = 0;
        if(eores_OK != eo_array_PushBack(array, &sigcfg))
            yError () << " EmbObj Analog Sensor while loading ropSig Array  at line " << __LINE__;
    }

    if(!res->transceiver->load_occasional_rop(eo_ropcode_set, endpoint_mn_comm, nvid_ropsigcfgassign))
        return false;
}


/*! Read a vector from the sensor.
 * @param out a vector containing the sensor's last readings.
 * @return AS_OK or return code. AS_TIMEOUT if the sensor timed-out.
 **/
int embObjAnalogSensor::read(yarp::sig::Vector &out)
{
    // This method gives data to the analogServer

      mutex.wait();

      if (!data)
      {
          mutex.post();
          return false;
      }

      // errors are not handled for now... it'll always be OK!!
       if (status!=IAnalogSensor::AS_OK)
      {
          switch (status)
          {
              case IAnalogSensor::AS_OVF:
                  {
                      counterSat++;
                  }
                  break;
              case IAnalogSensor::AS_ERROR:
                  {
                      counterError++;
                  }
                  break;
              case IAnalogSensor::AS_TIMEOUT:
                  {
                     counterTimeout++;
                  }
                  break;
              default:
              {
                  counterError++;
              }
          }
          mutex.post();
          return status;
      }

      out.resize(data->size());
      for(int k=0;k<data->size();k++)
      {
          out[k]=(*data)[k];
      }

      mutex.post();
    
    return status;
}

int embObjAnalogSensor::getState(int ch)
{
    printf("getstate\n");
    return AS_OK;
}

int embObjAnalogSensor::getChannels()
{
    return data->size();
}

int embObjAnalogSensor::calibrateSensor()
{
    return AS_OK;
}

int embObjAnalogSensor::calibrateSensor(const yarp::sig::Vector& value)
{
    return AS_OK;
}

int embObjAnalogSensor::calibrateChannel(int ch)
{
    return AS_OK;
}

int embObjAnalogSensor::calibrateChannel(int ch, double v)
{
    return AS_OK;
}

bool embObjAnalogSensor::fillData(void *as_array_raw)
{
    // Called by eoCallback.
//     yTrace();
    mutex.wait();
//     printf("\nembObj Analog Sensor fill_as_data\n");
    // do the decode16 code
    eOsnsr_arrayofupto12bytes_t  *as_array = (eOsnsr_arrayofupto12bytes_t*) as_array_raw;
    uint8_t msg[2];
    double *_buffer = data->getBuffer();

//     printf("_useCalibration: %d\n", _useCalibration);
    for(int k=0; k<_channels; k++)
    {
        // Get the kth element of the array as a 2 bytes msg
        memcpy(msg, (char*) eo_array_At((EOarray*) as_array, k), 2);
        // Got from canBusMotionControl
        _buffer[k]= (short)( ( (((unsigned short)(msg[1]))<<8)+msg[0]) - (unsigned short) (0x8000) );

        uint16_t testAAA = 0;
        uint16_t testBBB = 0;
        int16_t testCCC = 0;
        testAAA = msg[1];
        testBBB = testAAA << 8;
        testCCC = (int16_t) (testBBB + msg[0]);

//        if (k==4)
//        {
//        printf("0x%04X(%X %X) vs 0x%04X\n", (int16_t)_buffer[k], (uint8_t)msg[0], (uint8_t)msg[1], (uint16_t)testCCC);
//        printf("%d(%d %d) vs %d\n\n", (int16_t)_buffer[k], (uint8_t)msg[0], (uint8_t)msg[1], (uint16_t)testCCC);
//        }
//
        
        if (_useCalibration==1)
        {
            _buffer[k]=_buffer[k]*scaleFactor[k]/float(0x8000);
        }
    }
//     printf("\n");
    mutex.post();
    return AS_OK;
}
