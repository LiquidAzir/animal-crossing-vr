#include "JSystem/JSupport/JSUStream.h"

/* stdio's EOF macro is -1; the JSU end-of-stream state is the single bit 1. */
static const EIoState kJsuEof = static_cast<EIoState>(1);

JSUInputStream::~JSUInputStream() {
}

int JSUInputStream::read(void* buf, s32 size) {
    int len = this->readData(buf, size);
    if (len != size) {
        this->setState(kJsuEof);
    }
    return len;
}

char* JSUInputStream::read(char* str) {
    u16 size;
    int len = this->readData(&size, sizeof(size));
    if (len != sizeof(size)) {
        str[0] = '\0';
        this->setState(kJsuEof);
        str = nullptr;
    } else {
        int strRead = this->readData(str, size);
        str[strRead] = '\0';
        if (strRead != size) {
            this->setState(kJsuEof);
        }
    }

    return str;
}

/* @fabricated -- this method is confirmed to exist, but goes unused in AC */
char* JSUInputStream::readString() {
    u16 len;
    int r = this->readData(&len, sizeof(len));
    if (r != sizeof(len)) {
        this->setState(kJsuEof);
        return nullptr;
    }

    char* buf = new char[len + 1];
    r = this->readData(buf, len);
    if (r != len) {
        delete[] buf;
        this->setState(kJsuEof);
        return nullptr;
    }

    buf[len] = '\0';
    return buf;
}

/* @fabricated -- this method is confirmed to exist, but goes unused in AC */
char* JSUInputStream::readString(char* buf, u16 len) {
    int r = this->readData(buf, len);
    if (r != len) {
        this->setState(kJsuEof);
        return nullptr;
    }

    buf[len] = '\0';
    return buf;
}

int JSUInputStream::skip(s32 amount) {
    u8 _p;
    int i;

    for (i = 0; i < amount; i++) {
        if (this->readData(&_p, sizeof(_p)) != sizeof(_p)) {
            this->setState(kJsuEof);
            break;
        }
    }

    return i;
}

/* JSURandomInputStream */

int JSURandomInputStream::skip(s32 amount) {
    int s = this->seekPos(amount, static_cast<JSUStreamSeekFrom>(SEEK_CUR));
    if (s != amount) {
        this->setState(kJsuEof);
    }
    return s;
}

/* This method is confirmed to exist, but goes unused in AC. Retrieved from TP debug. */
int JSURandomInputStream::align(s32 alignment) {
    int pos = this->getPosition();
    int aligned = ((alignment - 1) + pos) & ~(alignment - 1);
    int change = aligned - pos;

    if (change != 0) {
        int s = this->seekPos(aligned, static_cast<JSUStreamSeekFrom>(SEEK_SET));
        if (s != change) {
            this->setState(kJsuEof);
        }
    }

    return change;
}

/* This method is confirmed to exist, but goes unused in AC. Retrieved from TP debug. */
int JSURandomInputStream::peek(void* buf, s32 len) {
    int pos = this->getPosition();
    int r = this->read(buf, len);
    if (r != 0) {
        this->seekPos(pos, static_cast<JSUStreamSeekFrom>(SEEK_SET));
    }

    return r;
}

int JSURandomInputStream::seek(s32 offset, JSUStreamSeekFrom from) {
    int s = this->seekPos(offset, from);
    this->clrState(kJsuEof);
    return s;
}
