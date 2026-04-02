#include "utils/stream.hpp"
#include <algorithm>
#include <cstring>

namespace zap {

ColorOverride color_override = ColorOverride::AUTO;

Stream::Stream(size_t bufferSize) {
  start = cur = end = nullptr;

  if (bufferSize) {
    setBufferSize(bufferSize);
  }
}

void Stream::setBufferSize(size_t bufferSize) {
  if (bufferSize == getBufferSize())
    return;

  flush();
  delete[] start;

  start = cur = end = nullptr;

  if (bufferSize) {
    start = cur = new BufferChar[bufferSize];
    end = start + bufferSize;
  }
}

Stream &Stream::write(const BufferChar *ptr, size_t size) {
  size_t bufferSize = getBufferSize();

  if (!bufferSize) {
    internalWrite(ptr, size);
    return *this;
  }

  size_t bytesLeft = size;
  while (bytesLeft) {
    size_t spaceLeft = end - cur;

    if (bytesLeft >= bufferSize) {
      flush();
      internalWrite(ptr, size);
      break;
    }

    if (!spaceLeft) {
      flush();
      spaceLeft = bufferSize;
    }

    size_t toCpy = std::min(bytesLeft, spaceLeft);

    std::memcpy(cur, ptr, toCpy);

    cur += toCpy;
    ptr += toCpy;
    bytesLeft -= toCpy;
  }

  return *this;
}

struct HandleColors {
  bool stdout_color;
  bool stdout_tty;
  bool stderr_color;
  bool stderr_tty;

  HandleColors();
};

static HandleColors standard_stream_colors;

class StdoutStream : public SFStream {
public:
  using SFStream::SFStream;

  bool hasColors() const override {
    return standard_stream_colors.stdout_color;
  }

  bool onTTY() const override { return standard_stream_colors.stdout_tty; }
};

class StderrStream : public SFStream {
public:
  using SFStream::SFStream;

  bool hasColors() const override {
    return standard_stream_colors.stderr_color;
  }

  bool onTTY() const override { return standard_stream_colors.stderr_tty; }
};

Stream &err() {
  static StderrStream stderrStream(stderr, false);
  return stderrStream;
}

Stream &out() {
  static StdoutStream stdoutStream(stdout, false);
  return stdoutStream;
}

/// This is where it is platform dependent.
HandleColors::HandleColors() {
  // Asssume colors do exist, ideally this should be implemented for every
  // platform.
  stdout_color = true;
  stdout_tty = true;

  stderr_color = true;
  stderr_tty = true;
}

} // namespace zap