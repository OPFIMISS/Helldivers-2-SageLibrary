#pragma once

/* Original, procedural mechanical reload cue; no game audio redistribution. */
static void rocket_reload_audio(void)
{
    typedef BOOL (WINAPI *Play)(LPCWSTR,HMODULE,DWORD);
    static Play play;
    static uint8_t wave[44+30870*2];
    static bool prepared;
    if (!play) {
        HMODULE module=GetModuleHandleA("winmm.dll");
        if (!module) module=LoadLibraryW(L"winmm.dll");
        if (module) play=(Play)GetProcAddress(module,"PlaySoundW");
    }
    if (!play) return;
    if (!prepared) {
        const uint32_t size=sizeof(wave)-8,data=sizeof(wave)-44,rate=22050,bytes=44100;
        const uint16_t pcm=1,channels=1,align=2,bits=16;
        memcpy(wave,"RIFF",4);memcpy(wave+4,&size,4);memcpy(wave+8,"WAVEfmt ",8);
        const uint32_t fmt=16;
        memcpy(wave+16,&fmt,4);memcpy(wave+20,&pcm,2);memcpy(wave+22,&channels,2);
        memcpy(wave+24,&rate,4);memcpy(wave+28,&bytes,4);memcpy(wave+32,&align,2);
        memcpy(wave+34,&bits,2);memcpy(wave+36,"data",4);memcpy(wave+40,&data,4);
        uint32_t random=0x524C4431;
        float low=0;
        const float cues[]={0.0f,0.18f,0.46f,0.78f,1.10f,1.31f};
        for (unsigned n=0;n<30870;++n) {
            const float t=n/22050.0f;
            random=random*1664525u+1013904223u;
            const float noise=(float)(random>>8)/8388608.0f-1;
            low+=0.12f*(noise-low);
            float sample=0;
            for (unsigned k=0;k<6;++k) {
                const float age=t-cues[k];
                if (age>=0 && age<0.12f) {
                    const float envelope=expf(-age*(k==5?48:70));
                    sample+=envelope*((noise-low)*0.09f+
                        sinf(age*6.2831853f*(510+k*113))*0.055f+
                        sinf(age*6.2831853f*(1480+k*137))*0.025f);
                }
            }
            const int16_t value=(int16_t)(fmaxf(-1,fminf(1,sample))*32767);
            memcpy(wave+44+n*2,&value,2);
        }
        prepared=true;
    }
    play((LPCWSTR)wave,NULL,0x0001u|0x0002u|0x0004u);
}

static void rocket_reload_audio_stop(void)
{
    HMODULE module=GetModuleHandleA("winmm.dll");
    if (module) {
        typedef BOOL (WINAPI *Play)(LPCWSTR,HMODULE,DWORD);
        Play play=(Play)GetProcAddress(module,"PlaySoundW");
        if (play) play(NULL,NULL,0);
    }
}
