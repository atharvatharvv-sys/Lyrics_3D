#pragma once
#include <jni.h>
#include <string>

class AudioPlayerJni {
public:
    void init(JavaVM* vm) { mVm = vm; }

    void playFile(const std::string& filePath) {
        JNIEnv* env = getEnv();
        if (!env) return;

        stopInternal(env);

        jclass cls  = env->FindClass("android/media/MediaPlayer");
        jmethodID ctor = env->GetMethodID(cls, "<init>", "()V");
        jobject mp  = env->NewObject(cls, ctor);

        jmethodID setDs   = env->GetMethodID(cls, "setDataSource", "(Ljava/lang/String;)V");
        jmethodID prepare = env->GetMethodID(cls, "prepare", "()V");
        jmethodID start   = env->GetMethodID(cls, "start", "()V");

        jstring jPath = env->NewStringUTF(filePath.c_str());
        env->CallVoidMethod(mp, setDs, jPath);
        env->DeleteLocalRef(jPath);

        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(mp);
            return;
        }

        env->CallVoidMethod(mp, prepare);
        env->CallVoidMethod(mp, start);
        mPlayer = env->NewGlobalRef(mp);
        env->DeleteLocalRef(mp);
    }

    void togglePause() {
        JNIEnv* env = getEnv();
        if (!env || !mPlayer) return;
        jclass cls = env->GetObjectClass(mPlayer);
        jmethodID isPlaying = env->GetMethodID(cls, "isPlaying", "()Z");
        if (env->CallBooleanMethod(mPlayer, isPlaying)) {
            jmethodID pause = env->GetMethodID(cls, "pause", "()V");
            env->CallVoidMethod(mPlayer, pause);
        } else {
            jmethodID start = env->GetMethodID(cls, "start", "()V");
            env->CallVoidMethod(mPlayer, start);
        }
    }

    double getPositionSeconds() {
        JNIEnv* env = getEnv();
        if (!env || !mPlayer) return 0.0;
        jclass cls = env->GetObjectClass(mPlayer);
        jmethodID getPos = env->GetMethodID(cls, "getCurrentPosition", "()I");
        jint ms = env->CallIntMethod(mPlayer, getPos);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return 0.0;
        }
        return static_cast<double>(ms) / 1000.0;
    }

private:
    JavaVM* mVm     = nullptr;
    jobject mPlayer = nullptr;

    JNIEnv* getEnv() {
        if (!mVm) return nullptr;
        JNIEnv* env = nullptr;
        if (mVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            mVm->AttachCurrentThread(&env, nullptr);
        }
        return env;
    }

    void stopInternal(JNIEnv* env) {
        if (!mPlayer) return;
        jclass cls = env->GetObjectClass(mPlayer);
        jmethodID stop    = env->GetMethodID(cls, "stop", "()V");
        jmethodID release = env->GetMethodID(cls, "release", "()V");
        env->CallVoidMethod(mPlayer, stop);
        env->CallVoidMethod(mPlayer, release);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(mPlayer);
        mPlayer = nullptr;
    }
};
