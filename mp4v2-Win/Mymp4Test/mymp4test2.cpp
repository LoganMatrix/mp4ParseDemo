#if defined( _WIN32 )
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 
#include <mp4v2/mp4v2.h>

#if defined _WIN32
#include "libplatform/platform_win32.h"
#endif


unsigned char sps[64], pps[64];
uint8_t aacAdtsHeader[7]; // adts header len: 7 bytes.
int spslen = 0, ppslen = 0;
uint32_t g264FrameLenSize = 0; // mp4���AVCC��ʽ��264֡�ĳ����ֶΣ�ռ�õ��ֽ�����һ����4.
uint8_t gAudioType = 0;
int gAudioChanNum = -1;
int gAudioRateIndex = 0;

int get264Stream(MP4FileHandle oMp4File, int VTrackId, int totalFrame);
int getAACStream(MP4FileHandle oMp4File, int ATrackId, int totalFrame);
int demuxMp4File(const char *sMp4file);
void fillAdtsHeader(uint8_t* aacAdtsHeader, int packetLen);

int mymp4test2(const char* mp4FileName)
{
    demuxMp4File(mp4FileName);
    return 0;
}


int get264Stream(MP4FileHandle oMp4File, int VTrackId, int totalFrame)
{
    if (!oMp4File) 
        return -1;

    char NAL[4] = { 0x00,0x00,0x00,0x01 };
    
    unsigned int nSize = 1024 * 1024 * 1;
    unsigned char *pData = (unsigned char *)malloc(nSize); // �������1MB��֡

    unsigned char *p = NULL;
    MP4Timestamp pStartTime;
    MP4Duration pDuration;
    MP4Duration pRenderingOffset;
    bool pIsSyncSample = 0;
    unsigned int frameSize = 0;
    bool firstWriteFile = true;

    int nReadIndex = 0;
    FILE *pFile = NULL;
    pFile = fopen("e:\\out1.264", "wb");

    while (nReadIndex < totalFrame)
    {
        nReadIndex++;
        //printf("nReadIndex:%d\n",nReadIndex);
        nSize = 1024 * 1024 * 1;
        MP4ReadSample(oMp4File, VTrackId, nReadIndex, &pData, &nSize, &pStartTime, &pDuration, &pRenderingOffset, &pIsSyncSample);

        // printf("I:%d, nSize:%u.   nReadIndex:%d\n", pIsSyncSample, nSize, nReadIndex);

        // IDR֡��д��sps pps��
        // ��������д�ļ�����ֻдһ�μ��ɣ�����������䣬�����ÿ��IDR֡д��.
        if (firstWriteFile && pIsSyncSample)
        {
            fwrite(NAL, sizeof(NAL), 1, pFile);
            fwrite(sps, spslen, 1, pFile);

            fwrite(NAL, sizeof(NAL), 1, pFile);
            fwrite(pps, ppslen, 1, pFile);

            firstWriteFile = false;
        }

        p = pData;
        while (nSize > 0) 
        {
            // ��׼��264֡��ǰ��4���ֽھ���frame�ĳ��ȣ�H264 AVCC��ʽ��
            // ��Ҫ��д���׼��264 nal ͷ.
            fwrite(NAL, sizeof(NAL), 1, pFile);

            frameSize = 0x0;
            if (g264FrameLenSize == 4)
            {
                frameSize |= *p << 24;
                frameSize |= *(p + 1) << 16;
                frameSize |= *(p + 2) << 8;
                frameSize |= *(p + 3);
            }
            else if (g264FrameLenSize == 2)
            {
                frameSize |= *p << 8;
                frameSize |= *(p + 1);
            }
            else if (g264FrameLenSize == 1)
            {
                frameSize |= *p;
            }
            else
            {
                printf("Err. The g264FrameLenSize is %d.\n", g264FrameLenSize);
                break;
            }
            
            // ��nSize�ݼ���ƫ�Ƶ���һ��֡��λ��
            nSize = nSize - frameSize - g264FrameLenSize;
            if(nSize < 0)
                printf("Err. I:%d, framesize=%x, nSize=%d\n", pIsSyncSample, frameSize, nSize);
            p = p + g264FrameLenSize; // �����ֶβ����
            fwrite(p, frameSize, 1, pFile);
            if (nSize > 0) // ��ʾ�˴�MP4ReadSample()��ȡ�����ݣ���ֹ1֡�����Լ�������
                p = p + frameSize;
        }

        // �������MP4ReadSample����ƵpData��null
        // ���ڲ��ͻ�new һ���ڴ�
        // ������������֪���ڴ�����
        // ����Ҫ��֤�ռ�bigger then max frames size.
        // free(p);
        // pData = NULL;
        // p = NULL;
    }
    fflush(pFile);
    fclose(pFile);

    free(pData);

    return 0;
}

int getAACStream(MP4FileHandle oMp4File, int ATrackId, int totalFrame)
{
    if (!oMp4File) 
        return -1;
    
    unsigned char *pData = NULL;
    // unsigned char *p = NULL;
    unsigned int i = 0;
    unsigned int nSize = 0;
    MP4Timestamp pStartTime;
    MP4Duration pDuration;
    MP4Duration pRenderingOffset;

    int nReadIndex = 0;
    FILE *pFile = NULL;
    pFile = fopen("E:\\out2.aac", "wb");

    while (nReadIndex < totalFrame)
    {
        nReadIndex++;

        MP4ReadSample(oMp4File, ATrackId, nReadIndex, &pData, &nSize, &pStartTime, &pDuration, &pRenderingOffset);
        // p = pData;
        fillAdtsHeader(aacAdtsHeader, nSize + sizeof(aacAdtsHeader));
        fwrite(aacAdtsHeader, sizeof(aacAdtsHeader), 1, pFile);
        fwrite(pData, nSize, 1, pFile);


        //�������MP4ReadSample����ƵpData��null
        // ���ڲ��ͻ�new һ���ڴ�
        //������������֪���ڴ�����
        //����Ҫ��֤�ռ�bigger then max frames size.
        free(pData);
        pData = NULL;
    }

    fflush(pFile);
    fclose(pFile);

    return 0;
}

// ADTS֡���Ȱ���ADTS���Ⱥ�AAC�������ݳ��ȵĺ�
// Ref: https://www.cnblogs.com/lidabo/p/7261558.html
void fillAdtsHeader(uint8_t* aacAdtsHeader, int packetLen)
{
    // fill in ADTS data
    aacAdtsHeader[0] = (uint8_t)0xFF;
    aacAdtsHeader[1] = (uint8_t)0xF1;
    aacAdtsHeader[2] = (uint8_t)(((gAudioType - 1) << 6) + (gAudioRateIndex << 2) + (gAudioChanNum >> 2));
    aacAdtsHeader[3] = (uint8_t)(((gAudioChanNum & 3) << 6) + (packetLen >> 11));
    aacAdtsHeader[4] = (uint8_t)((packetLen & 0x7FF) >> 3);
    aacAdtsHeader[5] = (uint8_t)(((packetLen & 7) << 5) + 0x1F);
    aacAdtsHeader[6] = (uint8_t)0xFC;
}

/* demuxMp4File
 * ����һ��mp4�ļ����ٶ���h264 + AAC�����ֱ�����Ƶ�洢����ͬĿ���ļ���
 * h264Ŀ���ļ���Annex B��ʽ����00 00 00 01ͷ�����֣����ο���
 * https://blog.csdn.net/romantic_energy/article/details/50508332
 * https://blog.csdn.net/lq496387202/article/details/81510622
 * aacĿ���ļ���AAC�����������adtsͷ�����ο���
 * https://www.cnblogs.com/lidabo/p/7261558.html
 * https://www.cnblogs.com/yanwei-wang/p/12758570.html
 * �����blog��https://blog.csdn.net/leixiaohua1020/article/details/39767055
*/
int demuxMp4File(const char *sMp4file)
{
    MP4FileHandle oMp4File;
    // int i;

    //unsigned int oStreamDuration;
    unsigned int vFrameCount;
    unsigned int aFrameCount;

    oMp4File = MP4Read(sMp4file);
    int videoindex = -1, audioindex = -1;
    uint32_t numSamples;
    //uint32_t timescale;
    //uint64_t duration;        

    if (!oMp4File)
    {
        printf("Err. Read %s is failed.\n", sMp4file);
        return -1;
    }

    MP4TrackId trackId = MP4_INVALID_TRACK_ID;
    uint32_t numTracks = MP4GetNumberOfTracks(oMp4File, NULL, 0);
    printf("numTracks:%d\n", numTracks);

    for (uint32_t i = 0; i < numTracks; i++)
    {
        trackId = MP4FindTrackId(oMp4File, i, NULL, 0);
        const char* trackType = MP4GetTrackType(oMp4File, trackId);
        if (MP4_IS_VIDEO_TRACK_TYPE(trackType))
        {
            //printf("[%s %d] trackId:%d\r\n",__FUNCTION__,__LINE__,trackId);
            videoindex = trackId;

            //duration = MP4GetTrackDuration(oMp4File, trackId );
            numSamples = MP4GetTrackNumberOfSamples(oMp4File, trackId);
            //timescale = MP4GetTrackTimeScale(oMp4File, trackId);          
            //oStreamDuration = duration/(timescale/1000);          
            vFrameCount = numSamples;

            // read sps/pps 
            uint8_t **seqheader;
            uint8_t **pictheader;
            uint32_t *pictheadersize;
            uint32_t *seqheadersize;
            uint32_t ix;

            MP4GetTrackH264SeqPictHeaders(oMp4File, trackId, &seqheader, &seqheadersize, &pictheader, &pictheadersize);

            /* Ref:
              mp4�洢H.264���ķ�ʽ��AVCC��ʽ��
              �����ָ�ʽ�У�ÿһ��NALU����������һ��ָ���䳤��(NALU����С)��ǰ׺(in big endian format��˸�ʽ)��
              ���ָ�ʽ�İ��ǳ����׽������������ָ�ʽȥ����Annex B��ʽ�е��ֽڶ������ԣ�����ǰ׺������1��2��4�ֽڣ�
              ����AVCC��ʽ��ø������ˣ�ָ��ǰ׺�ֽ���(1��2��4�ֽ�)��ֵ������һ��ͷ��������(����ʼ�Ĳ���)
            */
            MP4GetTrackH264LengthSize(oMp4File, trackId, &g264FrameLenSize);

            for (ix = 0; seqheadersize[ix] != 0; ix++)
            {
                memcpy(sps, seqheader[ix], seqheadersize[ix]);
                spslen = seqheadersize[ix];
                free(seqheader[ix]);
            }
            free(seqheader);
            free(seqheadersize);

            for (ix = 0; pictheadersize[ix] != 0; ix++)
            {
                memcpy(pps, pictheader[ix], pictheadersize[ix]);
                ppslen = pictheadersize[ix];
                free(pictheader[ix]);
            }
            free(pictheader);
            free(pictheadersize);
        }
        else if (MP4_IS_AUDIO_TRACK_TYPE(trackType))
        {
            audioindex = trackId;
            aFrameCount = MP4GetTrackNumberOfSamples(oMp4File, trackId);
            // ��ȡ��Ƶ������/������/������
            // ������ͼ� mp4v2��ģ�src/mp4info.cpp -> PrintAudioInfo()
            gAudioType = MP4GetTrackAudioMpeg4Type(oMp4File, trackId);
            gAudioChanNum = MP4GetTrackAudioChannels(oMp4File, trackId);
            uint32_t audio_time_scale = MP4GetTrackTimeScale(oMp4File, trackId);

            printf("audio: type %x, channel_num: %d, %u Hz\n", gAudioType, gAudioChanNum, audio_time_scale);

            //const char * audio_name = MP4GetTrackMediaDataName(inFile, id);
            //TRACE("=== ��Ƶ���룺%s ===\n", audio_name);
            
            // TRACE("=== ��Ƶÿ��̶���(������)��%lu ===\n", audio_time_scale);
            if (audio_time_scale == 48000)
                gAudioRateIndex = 0x03;
            else if (audio_time_scale == 44100)
                gAudioRateIndex = 0x04;
            else if (audio_time_scale == 32000)
                gAudioRateIndex = 0x05;
            else if (audio_time_scale == 24000)
                gAudioRateIndex = 0x06;
            else if (audio_time_scale == 22050)
                gAudioRateIndex = 0x07;
            else if (audio_time_scale == 16000)
                gAudioRateIndex = 0x08;
            else if (audio_time_scale == 12000)
                gAudioRateIndex = 0x09;
            else if (audio_time_scale == 11025)
                gAudioRateIndex = 0x0a;
            else if (audio_time_scale == 8000)
                gAudioRateIndex = 0x0b;

        }
    }

    // ��������mp4����ʼ������track����洢
    if (videoindex >= 0)
        get264Stream(oMp4File, videoindex, vFrameCount);
    if (audioindex >= 0)
        getAACStream(oMp4File, audioindex, aFrameCount);

    // ��Ҫmp4close
    MP4Close(oMp4File, 0);
    return 0;
}

