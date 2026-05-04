/**
 *-----------------------------------------------------------------------------
 * Company    : SLAC National Accelerator Laboratory
 *-----------------------------------------------------------------------------
 * Description: Device support for ADCs using the TI AM335X Linux driver
 * ----------------------------------------------------------------------------
 * This file is part of the devTiAm335XAdc package. It is subject to
 * the license terms in the LICENSE.txt file found in the top-level directory
 * of this distribution and at:
 *    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 * No part of the devTiAm335XAdc package, including this file, may be
 * copied, modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 * ----------------------------------------------------------------------------
**/
#define USE_TYPED_DSET 1
#include <epicsExport.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <dbCommon.h>
#include <epicsStdio.h>
#include <epicsStdlib.h>
#include <dbScan.h>
#include <epicsThread.h>

#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/poll.h>
#include <endian.h>

static long tiAm335XAdc_init(int after);
static long tiAm335XAdc_init_record(struct dbCommon* precord);
static long tiAm335XAdc_read_record(aiRecord* precord);
static long tiAm335XAdc_get_ioint_info(int cmd, struct dbCommon* precord, IOSCANPVT* ppvt);
static long tiAm335XAdc_linconv(aiRecord* precord, int after);
static int write_file(const char* file, const char* data);
static ssize_t read_file(const char* file, char* buf, size_t bufsize);
static void reader_thread(void* pvt);

const aidset devTiAm335XAdc = {
  .common = {
    .number = 6,
    .init = tiAm335XAdc_init,
    .get_ioint_info = tiAm335XAdc_get_ioint_info,
    .init_record = tiAm335XAdc_init_record,
  },
  .read_ai = tiAm335XAdc_read_record,
  .special_linconv = tiAm335XAdc_linconv,
};

epicsExportAddress(dset, devTiAm335XAdc);

struct adc_channel
{
  int offset; /* byte offset from the beginning of the interleaved adc data */
  int bytes;  /* number of bytes of data */
  int bits;   /* bit range of the ADC */
  int is_signed;
  int is_le;
  int shift;
  int chan;
  char buf[8];
  IOSCANPVT scan;
  struct adc_channel* next;
};

struct adc_buffer {
  int fd;
  int num; /* buffer number (buffer%d) */
  int dev; /* device number (device%d) */
  int length;
  int stride;
  struct adc_channel* channels;
  struct adc_channel** chmap;
  int numChannels;
  struct adc_buffer* next;
  void* scratch;
  size_t scratchSize;
  epicsMutexId mutex; /* guards ADC channel data */
};

static struct adc_buffer* buffers = NULL;
static int numBuffers = 0;

struct adc_dpvt
{
  struct adc_buffer* adc;
  struct adc_channel* chan;
  double slope;
  char path[256];
};

static struct adc_buffer*
find_or_create_buffer(int dev, int buff)
{
  struct adc_buffer* b = NULL;
  for (b = buffers; b; b = b->next) {
    if (b->num == buff && b->dev == dev)
      return b;
  }

  b = calloc(sizeof(struct adc_buffer), 1);
  b->fd = -1;
  b->next = buffers;
  b->num = buff;
  b->dev = dev;
  b->length = 256;
  b->mutex = epicsMutexCreate();
  buffers = b;
  numBuffers++;
  return b;
}

static struct adc_channel*
add_channel(struct adc_buffer* buffer)
{
  struct adc_channel* chan = calloc(sizeof(struct adc_channel), 1);
  chan->next = buffer->channels;
  buffer->channels = chan;
  buffer->numChannels++;
  return chan;
}

static long
tiAm335XAdc_init_record(struct dbCommon* precord)
{
  aiRecord* rec = (aiRecord*)precord;

  /* input links in the format:
   * @iio_device_num,buffer_number,channel
   */
  char buf[512];
  strncpy(buf, rec->inp.value.instio.string, sizeof(buf));

  const char* devNum = strtok(buf, ",");
  const char* bufNum = strtok(NULL, ",");
  const char* chNum = strtok(NULL, ",");
  const char* vrNum = strtok(NULL, ",");

  if (!devNum || !bufNum || !chNum || !vrNum) {
    printf("Invalid syntax for INP link\n");
    return S_dev_badInpType;
  }

  int dev = 0, buff = 0, chan = 0;
  double vref = 0;

  if (epicsParseInt32(devNum, &dev, 10, NULL) != 0) {
    printf("Invalid number for device\n");
    return S_dev_badInpType;
  }

  if (epicsParseInt32(bufNum, &buff, 10, NULL) != 0) {
    printf("Invalid number for buffer\n");
    return S_dev_badInpType;
  }

  if (epicsParseInt32(chNum, &chan, 10, NULL) != 0) {
    printf("Invalid channel number\n");
    return S_dev_badInpType;
  }

  if (epicsParseDouble(vrNum, &vref, NULL) != 0){ 
    printf("Invalid Vref\n");
    return S_dev_badInpType;
  }

  struct adc_dpvt* dpvt = calloc(sizeof(struct adc_dpvt), 1);
  rec->dpvt = dpvt;

  snprintf(
    dpvt->path,
    sizeof(dpvt->path),
    "/sys/bus/iio/devices/iio:device%d/buffer%d",
    dev,
    buff
  );

  char path[PATH_MAX];

  /* this buffer has maybe already been enabled and must be disabled before
   * we add new channels */
  snprintf(path, sizeof(path), "%s/enable", dpvt->path);
  write_file(path, "0");

  /* enable this channel for the buffer */
  snprintf(path, sizeof(path), "%s/in_voltage%d_en", dpvt->path, chan);
  if (!write_file(path, "1")) {
    printf("Failed to init channel %d\n", chan);
    rec->dpvt = NULL;
    free(dpvt);
    return S_dev_badBus;
  }

  /* parse out the data format */
  char fmtInfo[256];
  snprintf(path, sizeof(path), "%s/in_voltage%d_type", dpvt->path, chan);
  read_file(path, fmtInfo, sizeof(fmtInfo));

  char le[3], signedness;
  int bits, shift, adcbits;
  sscanf(
    /* le:u12/16>>0 */
    fmtInfo, "%2s:%c%u/%u>>%d",
    le, &signedness, &adcbits, &bits, &shift
  );

  /* compute slope for converting rval -> val.
   * Since this depends on bit precision of the ADC (obtained from sysfs),
   * we won't rely on ESLO from the DB.
   * Vi = (Vref * counts) / (range-1)
   */
  dpvt->slope = vref / (pow(2, adcbits) - 1);

  /* find or add a new buffer */
  struct adc_buffer* b = find_or_create_buffer(dev, buff);
  dpvt->adc = b;

  struct adc_channel* ch = add_channel(b);
  ch->bytes = bits / 8;
  ch->bits = adcbits;
  ch->shift = shift;
  ch->is_le = strcasecmp(le, "le");
  ch->is_signed = signedness != 'u';
  ch->chan = chan;
  dpvt->chan = ch;
  scanIoInit(&ch->scan);
  return 0;
}

/* swap bytes if necessary */
static void
data_swap(struct adc_channel* chan, char* buf, size_t s)
{
#if BYTE_ORDER == LITTLE_ENDIAN
  int need_swap = !chan->is_le;
#elif BYTE_ORDER == BIG_ENDIAN
  int need_swap = chan->is_le;
#else
  #error What strange machine are you using?
#endif

  if (!need_swap || s <= 1)
    return;

  char tmp[s];
  memcpy(tmp, buf, s);
  for (int i = 0; i < s; ++i) {
    buf[i] = tmp[s-i-1];
  }
}

static long
tiAm335XAdc_read_record(aiRecord* precord)
{
  struct adc_dpvt* dpvt = precord->dpvt;
  precord->pact = FALSE;

  epicsMutexLock(dpvt->adc->mutex);

  /* swap if the data is incompatible with host byte order */
  data_swap(dpvt->chan, dpvt->chan->buf, sizeof dpvt->chan->buf);

  uint64_t uv = 0;
  int64_t sv = 0;
  switch (dpvt->chan->bytes) {
  case 8:
    /* aiRecord.rval is only 4 bytes, but we'll just let it truncate */
    if (dpvt->chan->is_signed)
      sv = *(int64_t*)dpvt->chan->buf;
    else
      uv = *(uint64_t*)dpvt->chan->buf;
    break;
  case 4:
    if (dpvt->chan->is_signed)
      sv = *(int32_t*)dpvt->chan->buf;
    else
      uv = *(uint32_t*)dpvt->chan->buf;
    break;
  case 2:
    if (dpvt->chan->is_signed)
      sv = *(int16_t*)dpvt->chan->buf;
    else
      uv = *(uint16_t*)dpvt->chan->buf;
    break;
  case 1:
    if (dpvt->chan->is_signed)
      sv = *(int8_t*)dpvt->chan->buf;
    else
      uv = *(uint8_t*)dpvt->chan->buf;
    break;
  default:
    assert(!"Unsupported byte count");
  }

  epicsMutexUnlock(dpvt->adc->mutex);

  /* we aren't actually using rval, but it's nice to see the real adc counts */
  precord->rval = dpvt->chan->is_signed ? sv : uv;
  precord->udf = FALSE;

  precord->val = (dpvt->chan->is_signed ? sv : uv) >> dpvt->chan->shift;
  precord->val *= dpvt->slope;

  return 2; /* 2 = dont convert */
}

static long
tiAm335XAdc_get_ioint_info(int cmd, struct dbCommon* precord, IOSCANPVT* ppvt)
{
  aiRecord* rec = (aiRecord*)precord;
  struct adc_dpvt* dpvt = rec->dpvt;
  if (!dpvt)
    return -1;
  *ppvt = dpvt->chan->scan;
  return 0;
}

static long
tiAm335XAdc_linconv(aiRecord* precord, int after)
{
  return 0;
}

static long
tiAm335XAdc_init(int after)
{
  if (!after)
    return 0;

  if (!buffers)
    return 0; /* nothing to read. */

  /* Build an offset map for each chip, and kick off threads */
  for (struct adc_buffer* b = buffers; b; b = b->next) {
    /* build a channel map and fill out offsets */
    b->chmap = calloc(sizeof(struct adc_channel*), b->numChannels);
    int n = 0, off = 0;
    for (struct adc_channel* c = b->channels; c; c = c->next, ++n) {
      b->chmap[c->chan] = c;
      c->offset = off;
      off += c->bytes;
    }

    char base[256];
    snprintf(
      base, sizeof(base),
      "/sys/bus/iio/devices/iio:device%d/buffer%d",
      b->dev, b->num
    );

    char path[256];

    /* allocate scratch area for the buffer */
    b->stride = off;
    b->scratchSize = off * b->length;
    b->scratch = calloc(b->scratchSize, 1);

    /* configure buffer length */
    snprintf(path, sizeof(path), "%s/length", base);
    char data[32];
    snprintf(data, sizeof(data), "%d", b->length);
    write_file(path, data);

    /* activate the buffer */
    snprintf(path, sizeof(path), "%s/enable", base);
    write_file(path, "1");

    snprintf(path, sizeof(path), "/dev/iio:device%d", b->dev);
    b->fd = open(path, O_RDONLY);
    if (b->fd < 0) {
      perror("open");
    }
  }

  /* Kick off the worker for all ADCs */
  epicsThreadOpts opts = {
    .joinable = 1,
    .priority = epicsThreadPriorityMedium,
    .stackSize = epicsThreadStackMedium,
  };
  if (!epicsThreadCreateOpt("ADCREAD", reader_thread, NULL, &opts))
    return 1;
  return 0;
}

static int
write_file(const char* file, const char* data)
{
  int fd = open(file, O_RDWR);
  if (fd < 0) {
    printf("Cannot write %s: %s\n", file, strerror(errno));
    return 0;
  }
  write(fd, data, strlen(data));
  close(fd);
  return 1;
}

static ssize_t
read_file(const char* file, char* buf, size_t bufsize)
{
  int fd = open(file, O_RDONLY);
  ssize_t nr = read(fd, buf, bufsize);
  close(fd);
  return nr;
}

static void
read_process_buffer(struct adc_buffer* b)
{
  ssize_t nr = read(b->fd, b->scratch, b->scratchSize);
  if (nr == 0)
    return;
  
  epicsMutexLock(b->mutex);

  /* we should always be reading at least one "packet" of data */
  assert((nr % b->stride) == 0);

  /* horrible ugly debugging */
#if 0
  for (int i = 0; i < nr; ++i) {
    printf(" 0x%02X", ((uint8_t*)b->scratch)[i]);
    if (i > 0 && i % 16 == 0)
      printf("\n");
  }
  printf("\n");
#endif

  uint8_t* scratch = b->scratch;
  for (int pass = 0; pass < nr / b->stride; ++pass) {
    int off = 0;
    for (int i = 0; i < b->numChannels; ++i) {
      memcpy(b->chmap[i]->buf, scratch + off, b->chmap[i]->bytes);
      off += b->chmap[i]->bytes;
    }
  }

  for (int i = 0; i < b->numChannels; ++i) {
    scanIoRequest(b->chmap[i]->scan);
  }
  
  epicsMutexUnlock(b->mutex);
}

static void
reader_thread(void* pvt)
{
  struct adc_buffer** buffs = calloc(sizeof(struct adc_buffer*), numBuffers);
  struct pollfd* pollfds = calloc(sizeof(struct pollfd), numBuffers);

  int n = 0;
  for (struct adc_buffer* b = buffers; b; b = b->next, ++n) {
    buffs[n] = b;
    pollfds[n].fd = b->fd;
    pollfds[n].events |= POLLIN;
  }

  while (1) {
    /* wait forever for new data */
    poll(pollfds, n, -1);

    for (int i = 0; i < numBuffers; ++i) {
      if (!(pollfds[i].revents & POLLIN))
        continue;
      read_process_buffer(buffs[i]);
    }
  }
}