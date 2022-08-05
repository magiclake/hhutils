/*
                     ,----------------,              ,---------,
                ,-----------------------,          ,"        ,"|
              ,"                      ,"|        ,"        ,"  |
             +-----------------------+  |      ,"        ,"    |
             |  .-----------------.  |  |     +---------+      |
             |  | huhai@ubuntu:   |  |  |     | -==----'|      |
             |  |                 |  |  |     |         |      |
             |  |                 |  |  |/----|`---=    |      |
             |  |                 |  |  |   ,/|==== ooo |      ;
             |  |                 |  |  |  // |(((( [33]|    ,"
             |  `-----------------'  |," .;'| |((((     |  ,"
             +-----------------------+  ;;  | |         |,"
                /_)______________(_/  //'   | +---------+
           ___________________________/___  `,
          /  oooooooooooooooo  .o.  oooo /,   ,"-----------
         / ==ooooooooooooooo==.o.  ooo= //   ,`--{)B     ,"

 * @projectName   CSGMT
 * @Date:  2022-09-09
 * @LastEditTime:  2022-09-09
 * @LastEditors:
 * @FilePath:
 * @Description:
 */
#ifndef HUART_HPP
#define HUART_HPP

#include <stdio.h>
#include <string>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <mutex>
#include <sys/time.h>
#include <map>
#include <stdlib.h>
#include <chrono>
#define _HAL_HLOG__ 1
#ifdef _HAL_HLOG__
#include "../../hal_hlog.h"
#define HUART_DEBUG(fmt, ...) HalLogStdout(__LINE__, __FILE__, name.c_str(), fmt, ##__VA_ARGS__)
#endif
#ifndef HUART_DEBUG
#define HUART_DEBUG(fmt, ...) printf(  fmt "%d\n" ,  ##__VA_ARGS__, __LINE__)
#endif

class HUart
{
    int fd;
    std::string path;

    bool doOpen()
    {
        if(path.empty() )
        {
            HUART_DEBUG("path is empty");
            return false;
        }

        if(fd > 0)
        {
            HUART_DEBUG("fd > 0, already open");
            return true;
        }

        fd = open(path.c_str(), O_NONBLOCK| O_NOCTTY | O_RDWR, 0);
        if (fd < 0)
        {
            //HAL_DEBUG("open %s failed, errno = %d", szDevPath, errno);
            HUART_DEBUG("open %s failed, errno = %d %s", path.c_str(), errno, strerror(errno));
            return false;
        }
        HUART_DEBUG("open %s OK, fd(%d)", path.c_str(), fd);

        struct termios ios = {0};
        cfmakeraw(&ios);
        ios.c_cflag |= CLOCAL | CREAD;

        /* disable echo on serial ports */
        tcgetattr(fd, &ios );
        ios.c_lflag = 0;  /* disable ECHO, ICANON, etc... */


        ios.c_cflag &= ~CSIZE;
        ios.c_cflag |= CS8;

        ios.c_cflag &= ~CSTOPB;

        ios.c_cflag &= ~PARENB;

        /* 流控: NONE */
        ios.c_cflag &= ~CRTSCTS;
        ios.c_iflag &= ~(IXON | IXOFF);

        ios.c_cc[VTIME] = 1;
        ios.c_cc[VMIN] = 255;

        ios.c_iflag &= ~ICRNL;
        ios.c_iflag &= ~INLCR;
        ios.c_iflag |= IGNBRK;

        ios.c_oflag &= ~OCRNL;
        ios.c_oflag &= ~ONLCR;
        ios.c_oflag &= ~OPOST;

        ios.c_lflag &= ~ICANON;
        ios.c_lflag &= ~ISIG;
        ios.c_lflag &= ~IEXTEN;
        ios.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ECHOCTL|ECHOPRT|ECHOKE);

        tcsetattr(fd, TCSANOW, &ios );

        tcflush(fd, TCIFLUSH);

        set(9600,8,1,1);
        return true;
    }

public:
    HUart()
    {
        fd = -1;
    }
    virtual ~HUart()
    {
        sclose();
    }

    int getFd() const {return fd;}
    std::string name;

    bool sopen(const std::string &dev)
    {
        path = dev;
        return doOpen();
    }


    int ssend(const __uint8_t *buf, __uint32_t len)
    {
        int sendlen = 0;
        if (buf == NULL)
        {
            HUART_DEBUG("invalid send buf");
            return -1;
        }
        if (fd < 0)
        {
            HUART_DEBUG("serial not open:%d", fd);
            return -2;
        }

        sendlen = write(fd, buf, len);
        if ( sendlen == -1)
        {
            HUART_DEBUG("serial_send err:%s,fd[%d]", strerror(errno), fd);
            return -3;
        }
        if (sendlen == 0)
        {
            HUART_DEBUG("err happen,write return 0 :%s reopen port.", strerror(errno));
            sclose();
            doOpen();
        }
        return sendlen;
    }
    int srecv(__uint8_t *buf, __uint32_t len, int seconds, int useconds = 0)
    {

        int recvlen = 0;
        if (buf == NULL)
        {
            HUART_DEBUG("empty buf");
            return -1;
        }

        if(len == 0)
        {
            len = 1024;
        }

        auto start = std::chrono::steady_clock::now();
        auto leftms = seconds*1000 + useconds/1000;

#if true
        SELECT_START:
        fd_set readfs;
        struct timeval timeout;
        FD_ZERO(&readfs);
        FD_SET(fd, &readfs);
        timeout.tv_sec = seconds;
        timeout.tv_usec = useconds;
        auto ret = select(fd+1, &readfs, NULL, NULL, &timeout);
        if(ret == 0)
        {
            // timeout, means there is no event.
//            HUART_DEBUG("select timeout");
            return 0;
        }
        if ( ret == -1)
        {
            HUART_DEBUG("serial_recv err:%d\n",ret);
            return -2;
        }
        if (!FD_ISSET(fd, &readfs))
        {
            HUART_DEBUG( "FD_ISSET %u err",  fd);
            //not ready yet
            return 0;
        }
        uint32_t index = 0;
        int tryCount = 0;
        while(true)
        {
            auto toberead = len - index;
            if(toberead < 0)
            {
                break;
            }
            recvlen = read(fd, &buf[index], toberead);
            if (recvlen < 0)
            {
                if(errno == EAGAIN || errno == EINTR){
                    usleep(20*1000);
                    if(tryCount>3)
                    {
                        HUART_DEBUG("try   cnt >  3, break");
                        break;
                    }
                    HUART_DEBUG("try again: cnt(%d)", tryCount++);
                    break;
//                    usleep(1000*1000);
//                    continue;
                }
                HUART_DEBUG("read %u err(%d):%s close and reopen", fd, errno, strerror(errno));
                sclose();
                doOpen();
                return -3;
            }
            else if( recvlen > 0 )
            {
                HUART_DEBUG("read %d", recvlen);
                index += recvlen;
                usleep(20*1000);
                tryCount = 0;
            }
            else //(recvlen == 0)
            {
                break;
            }
        }

        auto end = std::chrono::steady_clock::now();
        auto past = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        if(index == 0 && past < leftms)
        {
            leftms -= past;
            seconds = leftms/1000;
            useconds = leftms%1000*1000000;
            goto SELECT_START;
        }

        return index;
#else

        auto tms = seconds * 1000 + useconds / 1000;
        auto start = std::chrono::steady_clock::now();
        while (true)
        {
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);

            recvlen = read(fd, buf, len);
            if (recvlen < 0)
            {
                if (recvlen == EAGAIN || recvlen == EINTR)
                {
                    continue;
                }
                HUART_DEBUG("read %u err, close and reopen", fd);
                sclose();
                doOpen();
                return -3;
            }
            if (recvlen > 0)
            {
                return recvlen;
            }
        }
        //    HLOGDBG_COMM( "serial_recv len %u",  recvlen);
        return recvlen;
#endif
    }

    void sflush()
    {

        if (fd != -1)
        {
            tcflush(fd, TCIFLUSH);
            tcflush(fd, TCOFLUSH);
        }
        return;
    }
    void sclose()
    {

        if (fd > 0)
        {
            close(fd);
            HUART_DEBUG("fd(%d) closed",fd);
        }
        fd = -1;
        return;
    }

    bool set(uint32_t baud, uint32_t databits,
             uint32_t stopbits, int parity)
    {

        struct termios newtio,oldtio;
        int iParity = parity;


        if ( tcgetattr( fd,&oldtio) != 0)
        {
            HUART_DEBUG("tcgetattr failed:%s",strerror(errno));
            return false;
        }
        bzero( &newtio, sizeof( newtio ) );

        std::map<speed_t, speed_t> bdmaps =
            {
                {300, B300},
                {600, B600},
                {1200, B1200},
                {2400, B2400},
                {4800, B4800},
                {9600, B9600},
                {19200, B19200},
                {38400, B38400},
                {115200, B115200},
            };

        auto bset = bdmaps.find(baud);
        if(bset == bdmaps.end())
        {
            HUART_DEBUG("unsupport baudrate failed:%d",baud);
            return false;
        }
        cfsetispeed(&newtio, bset->second);
        cfsetospeed(&newtio, bset->second);

        newtio.c_cflag |= CLOCAL | CREAD;
        newtio.c_cflag &= ~CSIZE;

        switch( databits )
        {
        case 7:
            newtio.c_cflag |= CS7;
            break;
        case 8:
            newtio.c_cflag |= CS8;
            break;
        }

        switch( iParity )
        {
        case 0:
            newtio.c_cflag &= ~PARENB;
            break;
        case 1:
            newtio.c_cflag |= PARENB;
            newtio.c_cflag |= PARODD;
            //newtio.c_iflag |= (INPCK | ISTRIP);
            break;
        default:
        case 2:
            //newtio.c_iflag |= (INPCK | ISTRIP);
            newtio.c_cflag |= PARENB;
            newtio.c_cflag &= ~PARODD;
            break;

        }

        if( stopbits == 1 )
        {
            newtio.c_cflag &= ~CSTOPB;
        }
        else if ( stopbits == 2 )
        {
            newtio.c_cflag |= CSTOPB;
        }
        else
        {
            HUART_DEBUG("invalid stopbits:%d,it was ignored ",stopbits);
        }

        if((tcsetattr(fd,TCSANOW,&newtio))!=0)
        {
            HUART_DEBUG("com set error:%s",strerror(errno));
            tcsetattr(fd,TCSANOW,&oldtio);
            tcflush(fd,TCIFLUSH);
            return false;
        }
        tcflush(fd,TCIFLUSH);
        HUART_DEBUG("set done!\n");
        return true;
    }
};

#endif // HUART_HPP
