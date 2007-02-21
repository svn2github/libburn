/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef LIBBURN_H
#define LIBBURN_H

/* Needed for  off_t  which is the (POSIX-ly) appropriate type for
   expressing a file or stream size.

   XXX we should enforce 64-bitness for off_t
   ts A61101 : this is usually done by the build system (if it is not broken)
*/
#include <sys/types.h>

#ifndef DOXYGEN

#if defined(__cplusplus)
#define BURN_BEGIN_DECLS \
	namespace burn { \
		extern "C" {
#define BURN_END_DECLS \
		} \
	}
#else
#define BURN_BEGIN_DECLS
#define BURN_END_DECLS
#endif

BURN_BEGIN_DECLS

#endif

/** References a physical drive in the system */
struct burn_drive;

/** References a whole disc */
struct burn_disc;

/** References a single session on a disc */
struct burn_session;

/** References a single track on a disc */
struct burn_track;

/* ts A61111 */
/** References a set of write parameters */
struct burn_write_opts;

/** Session format for normal audio or data discs */
#define BURN_CDROM	0
/** Session format for obsolete CD-I discs */
#define BURN_CDI	0x10
/** Session format for CDROM-XA discs */
#define BURN_CDXA	0x20

#define BURN_POS_END 100

/** Mask for mode bits */
#define BURN_MODE_BITS 127

/** Track mode - mode 0 data
    0 bytes of user data.  it's all 0s.  mode 0.  get it?  HAH
*/
#define BURN_MODE0		(1 << 0)
/** Track mode - mode "raw" - all 2352 bytes supplied by app
    FOR DATA TRACKS ONLY!
*/
#define BURN_MODE_RAW		(1 << 1)
/** Track mode - mode 1 data
    2048 bytes user data, and all the LEC money can buy
*/
#define BURN_MODE1		(1 << 2)
/** Track mode - mode 2 data
    defaults to formless, 2336 bytes of user data, unprotected
    | with a data form if required.
*/
#define BURN_MODE2		(1 << 3)
/** Track mode modifier - Form 1, | with MODE2 for reasonable results
    2048 bytes of user data, 4 bytes of subheader
*/
#define BURN_FORM1		(1 << 4)
/** Track mode modifier - Form 2, | with MODE2 for reasonable results
    lots of user data.  not much LEC.
*/
#define BURN_FORM2		(1 << 5)
/** Track mode - audio
    2352 bytes per sector.  may be | with 4ch or preemphasis.
    NOT TO BE CONFUSED WITH BURN_MODE_RAW
    Audio data must be 44100Hz 16bit stereo with no riff or other header at
    beginning.  Extra header data will cause pops or clicks.  Audio data should
    also be in little-endian byte order.  Big-endian audio data causes static.
*/
#define BURN_AUDIO		(1 << 6)
/** Track mode modifier - 4 channel audio. */
#define BURN_4CH		(1 << 7)
/** Track mode modifier - Digital copy permitted, can be set on any track.*/
#define BURN_COPY		(1 << 8)
/** Track mode modifier - 50/15uS pre-emphasis */
#define BURN_PREEMPHASIS	(1 << 9)
/** Input mode modifier - subcodes present packed 16 */
#define BURN_SUBCODE_P16	(1 << 10)
/** Input mode modifier - subcodes present packed 96 */
#define BURN_SUBCODE_P96	(1 << 11)
/** Input mode modifier - subcodes present raw 96 */
#define BURN_SUBCODE_R96	(1 << 12)

/** Possible disc writing style/modes */
enum burn_write_types
{
	/** Packet writing.
	      currently unsupported, (for DVD Incremental Streaming use TAO)
	*/
	BURN_WRITE_PACKET,

	/** With CD:                     Track At Once recording
	      2s gaps between tracks, no fonky lead-ins

	    With sequential DVD-R[W]:    Incremental Streaming
	    With DVD-RAM/+RW:            Random Writeable (used sequentially)
	    With overwriteable DVD-RW:   Rigid Restricted Overwrite 
	*/
	BURN_WRITE_TAO,

	/** With CD:                     Session At Once
	      Block type MUST be BURN_BLOCK_SAO
	      ts A70122: Currently not capable of mixing data and audio tracks.

	    With sequential DVD-R[W]:    Disc-at-once, DAO
	      Single session, single track, fixed size mandatory, (-dvd-compat)
	*/
	BURN_WRITE_SAO,

	/** With CD: Raw disc at once recording.
	      all subcodes must be provided by lib or user
	      only raw block types are supported
	*/
	BURN_WRITE_RAW,

	/** In replies this indicates that not any writing will work.
	    As parameter for inquiries it indicates that no particular write
            mode shall is specified.
	    Do not use for setting a write mode for burning. It won't work.
	*/
	BURN_WRITE_NONE
};

/** Data format to send to the drive */
enum burn_block_types
{
	/** sync, headers, edc/ecc provided by lib/user */
	BURN_BLOCK_RAW0 = 1,
	/** sync, headers, edc/ecc and p/q subs provided by lib/user */
	BURN_BLOCK_RAW16 = 2,
	/** sync, headers, edc/ecc and packed p-w subs provided by lib/user */
	BURN_BLOCK_RAW96P = 4,
	/** sync, headers, edc/ecc and raw p-w subs provided by lib/user */
	BURN_BLOCK_RAW96R = 8,
	/** only 2048 bytes of user data provided by lib/user */
	BURN_BLOCK_MODE1 = 256,
	/** 2336 bytes of user data provided by lib/user */
	BURN_BLOCK_MODE2R = 512,
	/** 2048 bytes of user data provided by lib/user
	    subheader provided in write parameters
	    are we ever going to support this shit?  I vote no.
	    (supposed to be supported on all drives...)
	*/
	BURN_BLOCK_MODE2_PATHETIC = 1024,
	/** 2048 bytes of data + 8 byte subheader provided by lib/user
	    hey, this is also dumb
	*/
	BURN_BLOCK_MODE2_LAME = 2048,
	/** 2324 bytes of data provided by lib/user
	    subheader provided in write parameters
	    no sir, I don't like it.
	*/
	BURN_BLOCK_MODE2_OBSCURE = 4096,
	/** 2332 bytes of data supplied by lib/user
	    8 bytes sub header provided in write parameters
	    this is the second least suck mode2, and is mandatory for
	    all drives to support.
	*/
	BURN_BLOCK_MODE2_OK = 8192,
	/** SAO block sizes are based on cue sheet, so use this. */
	BURN_BLOCK_SAO = 16384
};

/** Possible status' of the drive in regard to the disc in it. */
enum burn_disc_status
{
	/** The current status is not yet known */
	BURN_DISC_UNREADY,
	/** The drive holds a blank disc */
	BURN_DISC_BLANK,
	/** There is no disc at all in the drive */
	BURN_DISC_EMPTY,
	/** There is an incomplete disc in the drive */
	BURN_DISC_APPENDABLE,
	/** There is a disc with data on it in the drive */
	BURN_DISC_FULL,

	/* ts A61007 */
	/** The drive was not grabbed when the status was inquired */
	BURN_DISC_UNGRABBED,

	/* ts A61020 */
	/** The media seems not to be suitable for burning */
	BURN_DISC_UNSUITABLE
};


/** Possible data source return values */
enum burn_source_status
{
	/** The source is ok */
	BURN_SOURCE_OK,
	/** The source is at end of file */
	BURN_SOURCE_EOF,
	/** The source is unusable */
	BURN_SOURCE_FAILED
};


/** Possible busy states for a drive */
enum burn_drive_status
{
	/** The drive is not in an operation */
	BURN_DRIVE_IDLE,
	/** The library is spawning the processes to handle a pending
	    operation (A read/write/etc is about to start but hasn't quite
	    yet) */
	BURN_DRIVE_SPAWNING,
	/** The drive is reading data from a disc */
	BURN_DRIVE_READING,
	/** The drive is writing data to a disc */
	BURN_DRIVE_WRITING,
	/** The drive is writing Lead-In */
	BURN_DRIVE_WRITING_LEADIN,
	/** The drive is writing Lead-Out */
	BURN_DRIVE_WRITING_LEADOUT,
	/** The drive is erasing a disc */
	BURN_DRIVE_ERASING,
	/** The drive is being grabbed */
	BURN_DRIVE_GRABBING,

	/* ts A61102 */
	/** The drive gets written zeroes before the track payload data */
	BURN_DRIVE_WRITING_PREGAP,
	/** The drive is told to close a track (TAO only) */
	BURN_DRIVE_CLOSING_TRACK,
	/** The drive is told to close a session (TAO only) */
	BURN_DRIVE_CLOSING_SESSION,

	/* ts A61223 */
	/** The drive is formatting media */
	BURN_DRIVE_FORMATTING

};

    
/** Information about a track on a disc - this is from the q sub channel of the
    lead-in area of a disc.  The documentation here is very terse.
    See a document such as mmc3 for proper information.

    CAUTION : This structure is prone to future extension !

    Do not restrict your application to unsigned char with any counter like
    "session", "point", "pmin", ...
    Do not rely on the current size of a burn_toc_entry. 

    ts A70201 : DVD extension, see below
*/
struct burn_toc_entry
{
	/** Session the track is in */
	unsigned char session;
	/** Type of data.  for this struct to be valid, it must be 1 */
	unsigned char adr;
	/** Type of data in the track */
	unsigned char control;
	/** Zero.  Always.  Really. */
	unsigned char tno;
	/** Track number or special information */
	unsigned char point;
	unsigned char min;
	unsigned char sec;
	unsigned char frame;
	unsigned char zero;
	/** Track start time minutes for normal tracks */
	unsigned char pmin;
	/** Track start time seconds for normal tracks */
	unsigned char psec;
	/** Track start time frames for normal tracks */
	unsigned char pframe;

	/* Indicates wether extension data are valid and eventually override
	   older elements in this structure:
	     bit0= DVD extension is valid
	*/
	unsigned char extensions_valid;  

	/* ts A70201 : DVD extension.
	   If invalid the members are guaranteed to be 0. */
	/* Tracks and session numbers are 16 bit. Here are the high bytes. */
	unsigned char session_msb;
	unsigned char point_msb;
	/* pmin, psec, and pframe may be too small if DVD extension is valid */
	int start_lba; 
	/* min, sec, and frame may be too small if DVD extension is valid */
	int track_blocks;
	
};


/** Data source for tracks */
struct burn_source {
	/** Reference count for the data source. Should be 1 when a new source
            is created.  Increment it to take a reference for yourself. Use
            burn_source_free to destroy your reference to it. */
	int refcount;

	/** Read data from the source */
	int (*read)(struct burn_source *,
	                                   unsigned char *buffer,
	                                   int size);

	/** Read subchannel data from the source (NULL if lib generated) */
	int (*read_sub)(struct burn_source *,
	                                       unsigned char *buffer,
	                                       int size);

	/** Get the size of the source's data */
	off_t (*get_size)(struct burn_source *);

	/** Set the size of the source's data */
	int (*set_size)(struct burn_source *source, off_t size);

	/** Clean up the source specific data */
	void (*free_data)(struct burn_source *);

	/** Next source, for when a source runs dry and padding is disabled
	    THIS IS AUTOMATICALLY HANDLED, DO NOT TOUCH
	*/
	struct burn_source *next;

	/** Source specific data */
	void *data;
};


/** Information on a drive in the system */
struct burn_drive_info
{
	/** Name of the vendor of the drive */
	char vendor[9];
	/** Name of the drive */
	char product[17];
	/** Revision of the drive */
	char revision[5];
	/** Location of the drive in the filesystem. */
	char location[17];
	/** This is currently the string which is used as persistent
	    drive address. But be warned: there is NO GUARANTEE that this
	    will stay so. Always use function  burn_drive_get_adr() to
	    inquire a persistent address.       ^^^^^^ ALWAYS ^^^^^^ */

	/** Can the drive read DVD-RAM discs */
	unsigned int read_dvdram:1;
	/** Can the drive read DVD-R discs */
	unsigned int read_dvdr:1;
	/** Can the drive read DVD-ROM discs */
	unsigned int read_dvdrom:1;
	/** Can the drive read CD-R discs */
	unsigned int read_cdr:1;
	/** Can the drive read CD-RW discs */
	unsigned int read_cdrw:1;

	/** Can the drive write DVD-RAM discs */
	unsigned int write_dvdram:1;
	/** Can the drive write DVD-R discs */
	unsigned int write_dvdr:1;
	/** Can the drive write CD-R discs */
	unsigned int write_cdr:1;
	/** Can the drive write CD-RW discs */
	unsigned int write_cdrw:1;

	/** Can the drive simulate a write */
	unsigned int write_simulate:1;

	/** Can the drive report C2 errors */
	unsigned int c2_errors:1;

	/** The size of the drive's buffer (in kilobytes) */
	int buffer_size;
	/** 
	 * The supported block types in tao mode.
	 * They should be tested with the desired block type.
	 * See also burn_block_types.
	 */
	int tao_block_types;
	/** 
	 * The supported block types in sao mode.
	 * They should be tested with the desired block type.
	 * See also burn_block_types.
	 */
	int sao_block_types;
	/** 
	 * The supported block types in raw mode.
	 * They should be tested with the desired block type.
	 * See also burn_block_types.
	 */
	int raw_block_types;
	/** 
	 * The supported block types in packet mode.
	 * They should be tested with the desired block type.
	 * See also burn_block_types.
	 */
	int packet_block_types;

	/** The value by which this drive can be indexed when using functions
	    in the library. This is the value to pass to all libbburn functions
	    that operate on a drive. */
	struct burn_drive *drive;
};


/** Operation progress report. All values are 0 based indices. 
 * */
struct burn_progress {
	/** The total number of sessions */
	int sessions;
	/** Current session.*/
	int session;
	/** The total number of tracks */
	int tracks;
	/** Current track. */
	int track;
	/** The total number of indices */
	int indices;
	/** Curent index. */
	int index;
	/** The starting logical block address */
	int start_sector;
	/** On write: The number of sectors.
	    On blank: 0x10000 as upper limit for relative progress steps */
	int sectors;
	/** On write: The current sector being processed.
	    On blank: Relative progress steps 0 to 0x10000 */
	int sector;

	/* ts A61023 */
	/** The capacity of the drive buffer */
	unsigned buffer_capacity;
	/** The free space in the drive buffer (might be slightly outdated) */
	unsigned buffer_available;

	/* ts A61119 */
	/** The number of bytes sent to the drive buffer */
	off_t buffered_bytes;
	/** The minimum number of buffered bytes. (Caution: Before surely
	    one buffer size of bytes was processed, this value is 0xffffffff.) 
	*/
	unsigned buffer_min_fill;
};


/* ts A61226 */
/** Description of a speed capability as reported by the drive in conjunction
    with eventually loaded media. There can be more than one such object per
    drive. So they are chained via .next and .prev , where NULL marks the end
    of the chain. This list is set up by burn_drive_scan() and gets updated
    by burn_drive_grab().
    A copy may be obtained by burn_drive_get_speedlist() and disposed by
    burn_drive_free_speedlist().
    For technical background info see SCSI specs MMC and SPC:
    mode page 2Ah (from SPC 5Ah MODE SENSE) , mmc3r10g.pdf , 6.3.11 Table 364
    ACh GET PERFORMANCE, Type 03h , mmc5r03c.pdf , 6.8.5.3 Table 312
*/
struct burn_speed_descriptor {

	/** Where this info comes from : 
	    0 = misc , 1 = mode page 2Ah , 2 = ACh GET PERFORMANCE */
	int source;

	/** The media type that was current at the time of report
	    -2 = state unknown, -1 = no media was loaded , else see
	    burn_disc_get_profile() */
	int profile_loaded;
	char profile_name[80];

	/** The attributed capacity of appropriate media in logical block units
	    i.e. 2352 raw bytes or 2048 data bytes. -1 = capacity unknown. */
	int end_lba;

	/** Speed is given in 1000 bytes/s , 0 = invalid. The numbers
	    are supposed to be usable with burn_drive_set_speed() */
	int write_speed;
	int read_speed;

	/** Expert info from ACh GET PERFORMANCE and/or mode page 2Ah.
	    Expect values other than 0 or 1 to get a meaning in future.*/
	/* Rotational control: 0 = CLV/default , 1 = CAV */
	int wrc;
	/* 1 = drive promises reported performance over full media */
	int exact;
	/* 1 = suitable for mixture of read and write */
	int mrw;

	/** List chaining. Use .next until NULL to iterate over the list */
	struct burn_speed_descriptor *prev;
	struct burn_speed_descriptor *next;
};


/** Initialize the library.
    This must be called before using any other functions in the library. It
    may be called more than once with no effect.
    It is possible to 'restart' the library by shutting it down and
    re-initializing it. This is necessary if you follow the older and
    more general way of accessing a drive via burn_drive_scan() and
    burn_drive_grab(). See burn_drive_scan_and_grab() with its strong
    urges and its explanations.
    @return Nonzero if the library was able to initialize; zero if
            initialization failed.
*/
int burn_initialize(void);

/** Shutdown the library.
    This should be called before exiting your application. Make sure that all
    drives you have grabbed are released <i>before</i> calling this.
*/
void burn_finish(void);


/* ts A61002 */
/** Abort any running drive operation and finally call burn_finish().
    You MUST calm down the busy drive if an aborting event occurs during a
    burn run. For that you may call this function either from your own signal
    handling code or indirectly by activating the builtin signal handling:
      burn_set_signal_handling("my_app_name : ", NULL, 0);
    Else you may eventually call burn_drive_cancel() on the active drive and
    wait for it to assume state BURN_DRIVE_IDLE.
    @param patience Maximum number of seconds to wait for drives to finish
    @param pacifier_func If not NULL: a function to produce appeasing messages.
                         See burn_abort_pacifier() for an example.
    @param handle Opaque handle to be used with pacifier_func
    @return 1  ok, all went well
            0  had to leave a drive in unclean state
            <0 severe error, do no use libburn again
*/
int burn_abort(int patience, 
               int (*pacifier_func)(void *handle, int patience, int elapsed),
               void *handle);

/** A pacifier function suitable for burn_abort.
    @param handle If not NULL, a pointer to a text suitable for printf("%s")
    @param patience Maximum number of seconds to wait
    @param elapsed  Elapsed number of seconds
*/
int burn_abort_pacifier(void *handle, int patience, int elapsed);


/** ts A61006 : This is for development only. Not suitable for applications.
    Set the verbosity level of the library. The default value is 0, which means
    that nothing is output on stderr. The more you increase this, the more
    debug output should be displayed on stderr for you.
    @param level The verbosity level desired. 0 for nothing, higher positive
                 values for more information output.
*/
void burn_set_verbosity(int level);

/* ts A60813 */
/** Set parameters for behavior on opening device files. To be called early
    after burn_initialize() and before any bus scan. But not mandatory at all.
    Parameter value 1 enables a feature, 0 disables.  
    Default is (1,0,0). Have a good reason before you change it.
    @param exclusive 1 = Try to open only devices which are not marked as busy
                     and try to mark them busy if opened sucessfully. (O_EXCL)
                     There are kernels which simply don't care about O_EXCL.
                     Some have it off, some have it on, some are switchable.
                     2 = in case of a SCSI device, also try to open exclusively
                     the matching /dev/sr, /dev/scd and /dev/st .
                     0 = no attempt to make drive access exclusive.
    @param blocking  Try to wait for drives which do not open immediately but
                     also do not return an error as well. (O_NONBLOCK)
                     This might stall indefinitely with /dev/hdX hard disks.
    @param abort_on_busy  Unconditionally abort process when a non blocking
                          exclusive opening attempt indicates a busy drive.
                          Use this only after thorough tests with your app.
*/
void burn_preset_device_open(int exclusive, int blocking, int abort_on_busy);


/* ts A60823 */
/** Aquire a drive with known persistent address.This is the sysadmin friendly
    way to open one drive and to leave all others untouched. It bundles
    the following API calls to form a non-obtrusive way to use libburn:
      burn_drive_add_whitelist() , burn_drive_scan() , burn_drive_grab()
    You are *strongly urged* to use this call whenever you know the drive
    address in advance.
    If not, then you have to use directly above calls. In that case, you are
    *strongly urged* to drop any unintended drive which will be exclusively
    occupied and not closed by burn_drive_scan().
    This can be done by shutting down the library including a call to
    burn_finish(). You may later start a new libburn session and should then
    use the function described here with an address obtained after
    burn_drive_scan() via burn_drive_get_adr(&(drive_infos[driveno]), adr) .
    Another way is to drop the unwanted drives by burn_drive_info_forget().
    @param drive_infos On success returns a one element array with the drive
                  (cdrom/burner). Thus use with driveno 0 only. On failure
                  the array has no valid elements at all.
                  The returned array should be freed via burn_drive_info_free()
                  when it is no longer needed, and before calling a scan
                  function again.
                  This is a result from call burn_drive_scan(). See there.
                  Use with driveno 0 only.
    @param adr    The persistent address of the desired drive. Either obtained
                  by burn_drive_get_adr() or guessed skillfully by application
                  resp. its user.
    @param load   Nonzero to make the drive attempt to load a disc (close its
                  tray door, etc).
    @return       1 = success , 0 = drive not found , -1 = other error
*/    
int burn_drive_scan_and_grab(struct burn_drive_info *drive_infos[],
                             char* adr, int load);


/* ts A51221 */
/** Maximum number of particularly permissible drive addresses */
#define BURN_DRIVE_WHITELIST_LEN 255
/** Add a device to the list of permissible drives. As soon as some entry is in
    the whitelist all non-listed drives are banned from scanning.
    @return 1 success, <=0 failure
*/
int burn_drive_add_whitelist(char *device_address);

/** Remove all drives from whitelist. This enables all possible drives. */
void burn_drive_clear_whitelist(void);


/** Scan for drives. This function MUST be called until it returns nonzero.
    No drives may be in use when this is called.
    All drive pointers are invalidated by using this function. Do NOT store
    drive pointers across calls to this function or death AND pain will ensue.
    After this call all drives depicted by the returned array are subject
    to eventual (O_EXCL) locking. See burn_preset_device_open(). This state
    ends either with burn_drive_info_forget() or with burn_drive_release().
    It is unfriendly to other processes on the system to hold drives locked
    which one does not definitely plan to use soon.
    @param drive_infos Returns an array of drive info items (cdroms/burners).
                  The returned array must be freed by burn_drive_info_free()
                  before burn_finish(), and also before calling this function
                  burn_drive_scan() again.
    @param n_drives Returns the number of drive items in drive_infos.
    @return 0 while scanning is not complete
            >0 when it is finished sucessfully,
            <0 when finished but failed.
*/
int burn_drive_scan(struct burn_drive_info *drive_infos[],
		    unsigned int *n_drives);

/* ts A60904 : ticket 62, contribution by elmom */
/** Release memory about a single drive and any exclusive lock on it.
    Become unable to inquire or grab it. Expect FATAL consequences if you try.
    @param drive_info pointer to a single element out of the array
                      obtained from burn_drive_scan() : &(drive_infos[driveno])
    @param force controls degree of permissible drive usage at the moment this
                 function is called, and the amount of automatically provided
                 drive shutdown : 
                  0= drive must be ungrabbed and BURN_DRIVE_IDLE
                  1= try to release drive resp. accept BURN_DRIVE_GRABBING 
                 Use these two only. Further values are to be defined.
    @return 1 on success, 2 if drive was already forgotten,
            0 if not permissible, <0 on other failures, 
*/
int burn_drive_info_forget(struct burn_drive_info *drive_info, int force);


/** When no longer needed, free a whole burn_drive_info array which was
    returned by burn_drive_scan().
    For freeing single drive array elements use burn_drive_info_forget().
*/
void burn_drive_info_free(struct burn_drive_info drive_infos[]);


/* ts A60823 */
/** Maximum length+1 to expect with a persistent drive address string */
#define BURN_DRIVE_ADR_LEN 1024

/** Inquire the persistent address of the given drive.
    @param drive_info The drive to inquire. Usually some &(drive_infos[driveno])
    @param adr   An application provided array of at least BURN_DRIVE_ADR_LEN
                 characters size. The persistent address gets copied to it.
    @return >0 success , <=0 error (due to libburn internal problem)
*/
int burn_drive_get_adr(struct burn_drive_info *drive_info, char adr[]);

/* ts A60922 ticket 33 */
/** Evaluate wether the given address would be a possible persistent drive
    address of libburn.
    @return 1 means yes, 0 means no
*/
int burn_drive_is_enumerable_adr(char *adr);

/* ts A60922 ticket 33 */
/** Try to convert a given existing filesystem address into a persistent drive
    address. This succeeds with symbolic links or if a hint about the drive's
    system address can be read from the filesystem object and a matching drive
    is found.
    @param path The address of an existing file system object
    @param adr  An application provided array of at least BURN_DRIVE_ADR_LEN
                characters size. The persistent address gets copied to it.
    @return     1 = success , 0 = failure , -1 = severe error
*/
int burn_drive_convert_fs_adr(char *path, char adr[]);

/* ts A60923 */
/** Try to convert a given SCSI address of bus,host,channel,target,lun into
    a persistent drive address. If a SCSI address component parameter is < 0
    then it is not decisive and the first enumerated address which matches
    the >= 0 parameters is taken as result.
    Note: bus and (host,channel) are supposed to be redundant.
    @param bus_no "Bus Number" (something like a virtual controller)
    @param host_no "Host Number" (something like half a virtual controller)
    @param channel_no "Channel Number" (other half of "Host Number")
    @param target_no "Target Number" or "SCSI Id" (a device)
    @param lun_no "Logical Unit Number" (a sub device)
    @param adr  An application provided array of at least BURN_DRIVE_ADR_LEN
                characters size. The persistent address gets copied to it.
    @return     1 = success , 0 = failure , -1 = severe error
*/
int burn_drive_convert_scsi_adr(int bus_no, int host_no, int channel_no,
				 int target_no, int lun_no, char adr[]);

/* ts A60923 - A61005 */
/** Try to obtain bus,host,channel,target,lun from path. If there is an SCSI
    address at all, then this call should succeed with a persistent
    drive address obtained via burn_drive_get_adr(). It is also supposed to
    succeed with any device file of a (possibly emulated) SCSI device.
    @return     1 = success , 0 = failure , -1 = severe error
*/
int burn_drive_obtain_scsi_adr(char *path, int *bus_no, int *host_no,
				int *channel_no, int *target_no, int *lun_no);

/** Grab a drive. This must be done before the drive can be used (for reading,
    writing, etc).
    @param drive The drive to grab. This is found in a returned
                 burn_drive_info struct.
    @param load Nonzero to make the drive attempt to load a disc (close its
                tray door, etc).
    @return 1 if it was possible to grab the drive, else 0
*/
int burn_drive_grab(struct burn_drive *drive, int load);


/** Release a drive. This should not be done until the drive is no longer
    busy (see burn_drive_get_status). The drive is (O_EXCL) unlocked
    afterwards.
	@param drive The drive to release.
	@param eject Nonzero to make the drive eject the disc in it.
*/
void burn_drive_release(struct burn_drive *drive, int eject);


/** Returns what kind of disc a drive is holding. This function may need to be
    called more than once to get a proper status from it. See burn_disc_status
    for details.
    @param drive The drive to query for a disc.
    @return The status of the drive, or what kind of disc is in it.
            Note: BURN_DISC_UNGRABBED indicates wrong API usage
*/
enum burn_disc_status burn_disc_get_status(struct burn_drive *drive);


/* ts A61020 */
/** WARNING: This revives an old bug-like behavior that might be dangerous.
    Sets the drive status to BURN_DISC_BLANK if it is BURN_DISC_UNREADY
    or BURN_DISC_UNSUITABLE. Thus marking media as writable which actually
    failed to declare themselves either blank or (partially) filled.
    @return 1 drive status has been set , 0 = unsuitable drive status
*/
int burn_disc_pretend_blank(struct burn_drive *drive);


/* ts A61106 */
/** WARNING: This overrides the safety measures against unsuitable media.
    Sets the drive status to BURN_DISC_FULL if it is BURN_DISC_UNREADY
    or BURN_DISC_UNSUITABLE. Thus marking media as blankable which actually
    failed to declare themselves either blank or (partially) filled.
*/
int burn_disc_pretend_full(struct burn_drive *drive);


/* ts A61021 */
/** Reads ATIP information from inserted media. To be obtained via
    burn_drive_get_write_speed(), burn_drive_get_min_write_speed(),
    burn_drive_get_start_end_lba(). The drive must be grabbed for this call.
    @param drive The drive to query.
    @return 1=sucess, 0=no valid ATIP info read, -1 severe error
*/
int burn_disc_read_atip(struct burn_drive *drive);


/* ts A61020 */
/** Returns start and end lba of the media which is currently inserted
    in the given drive. The drive has to be grabbed to have hope for reply.
    Shortcomming (not a feature): unless burn_disc_read_atip() was called 
    only blank media will return valid info.
    @param drive The drive to query.
    @param start_lba Returns the start lba value
    @param end_lba Returns the end lba value
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return 1 if lba values are valid , 0 if invalid
*/
int burn_drive_get_start_end_lba(struct burn_drive *drive,
                                 int *start_lba, int *end_lba, int flag);

/* ts A61110 */
/** Read start lba and Next Writeable Address of a track from media.
    Usually a track lba is obtained from the result of burn_track_get_entry().
    This call retrieves an updated lba, eventual nwa, and can address the
    invisible track to come.
    The drive must be grabbed for this call. One may not issue this call
    during ongoing burn_disc_write() or burn_disc_erase().
    @param d The drive to query.
    @param o If not NULL: write parameters to be set on drive before query
    @param trackno 0=next track to come, >0 number of existing track
    @param lba return value: start lba
    @param nwa return value: Next Writeable Address
    @return 1=nwa is valid , 0=nwa is not valid , -1=error
*/
int burn_disc_track_lba_nwa(struct burn_drive *d, struct burn_write_opts *o,
				int trackno, int *lba, int *nwa);

/* ts A70131 */
/** Read start lba of the first track in the last complete session.
    This is the first parameter of mkisofs option -C. The second parameter
    is nwa as obtained by burn_disc_track_lba_nwa() with trackno 0.
    @param d The drive to query.
    @param start_lba returns the start address of that track
    @return <= 0 : failure, 1 = ok 
*/
int burn_disc_get_msc1(struct burn_drive *d, int *start_lba);


/* ts A70213 */
/** Return the best possible estimation of the currently available capacity of
    the media. This might depend on particular write option settings. For
    inquiring the space with such a set of options, the drive has to be
    grabbed and BURN_DRIVE_IDLE. If not, then one will only get a canned value
    from the most recent automatic inquiry (e.g. during last drive grabbing).
    @param d The drive to query.
    @param o If not NULL: write parameters to be set on drive before query
    @return number of most probably available free bytes
*/
off_t burn_disc_available_space(struct burn_drive *d,
                                struct burn_write_opts *o);


/* ts A61202 */
/** Tells the MMC Profile identifier of the loaded media. The drive must be
    grabbed in order to get a non-zero result.
    libburn currently writes only to profiles 0x09 "CD-R", 0x0a "CD-RW",
    0x11 "DVD-R", 0x12 "DVD-RAM", 0x13 "DVD-RW restricted overwrite",
    0x14 "DVD-RW Sequential Recording" or 0x1a "DVD+RW".
    @param d The drive where the media is inserted.
    @param pno Profile Number as of mmc5r03c.pdf, table 89
    @param name Profile Name (e.g "CD-RW", unknown profiles have empty name)
    @return 1 profile is valid, 0 no profile info available 
*/
int burn_disc_get_profile(struct burn_drive *d, int *pno, char name[80]);

/** Tells whether a disc can be erased or not
    @return Non-zero means erasable
*/
int burn_disc_erasable(struct burn_drive *d);

/** Returns the progress and status of a drive.
    @param drive The drive to query busy state for.
    @param p Returns the progress of the operation, NULL if you don't care
    @return the current status of the drive. See also burn_drive_status.
*/
enum burn_drive_status burn_drive_get_status(struct burn_drive *drive,
					     struct burn_progress *p);

/** Creates a write_opts struct for burning to the specified drive
    must be freed with burn_write_opts_free
    @param drive The drive to write with
    @return The write_opts, NULL on error
*/
struct burn_write_opts *burn_write_opts_new(struct burn_drive *drive);

/** Frees a write_opts struct created with burn_write_opts_new
    @param opts write_opts to free
*/
void burn_write_opts_free(struct burn_write_opts *opts);

/** Creates a read_opts struct for reading from the specified drive
    must be freed with burn_read_opts_free
    @param drive The drive to read from
    @return The read_opts
*/
struct burn_read_opts *burn_read_opts_new(struct burn_drive *drive);

/** Frees a read_opts struct created with burn_read_opts_new
    @param opts write_opts to free
*/
void burn_read_opts_free(struct burn_read_opts *opts);

/** Erase a disc in the drive. The drive must be grabbed successfully BEFORE
    calling this functions. Always ensure that the drive reports a status of
    BURN_DISC_FULL before calling this function. An erase operation is not
    cancellable, as control of the operation is passed wholly to the drive and
    there is no way to interrupt it safely.
    @param drive The drive with which to erase a disc.
    @param fast Nonzero to do a fast erase, where only the disc's headers are
                erased; zero to erase the entire disc.
*/
void burn_disc_erase(struct burn_drive *drive, int fast);


/* ts A70101 - A70112 */
/** Format media for use with libburn. This currently applies to DVD-RW
    in state "Sequential Recording" (profile 0014h) which get formatted to
    state "Restricted Overwrite" (profile 0013h). DVD+RW can be "de-iced"
    by setting bit2 of flag. Other media cannot be formatted yet. 
    @param drive The drive with the disc to format.
    @param size The size in bytes to be used with the format command. It should
                be divisible by 32*1024. The effect of this parameter may
                depend on the media profile.
    @param flag Bitfield for control purposes:
                bit0= after formatting, write the given number of zero-bytes
                      to the media and eventually perform preliminary closing.
                bit1= insist in size 0 even if there is a better default known
                bit2= format to maximum available size
                bit3= -reserved-
                bit4= enforce re-format of (partly) formatted media
                bit7= MMC expert application mode (else libburn tries to
                      choose a suitable format type):
                      bit8 to bit15 contain the index of the format to use. See
                      burn_disc_get_formats(), burn_disc_get_format_descr().
                      Acceptable types are: 0x00, 0x10, 0x11, 0x13, 0x15, 0x26.
                      If bit7 is set, bit4 is set automatically.
*/
void burn_disc_format(struct burn_drive *drive, off_t size, int flag);


/* ts A70112 */
/** Possible formatting status values */
#define BURN_FORMAT_IS_UNFORMATTED 1
#define BURN_FORMAT_IS_FORMATTED   2
#define BURN_FORMAT_IS_UNKNOWN     3

/** Inquire the formatting status, the associated sizes and the number of
    available formats.  The info is media specific and stems from MMC command
    23h READ FORMAT CAPACITY. See mmc5r03c.pdf 6.24 for background details.
    Media type can be determined via burn_disc_get_profile().
    @param drive The drive with the disc to format.
    @param status The current formatting status of the inserted media.
                  See BURN_FORMAT_IS_* macros. Note: "unknown" is the
                  legal status for quick formatted, yet unwritten DVD-RW.
    @param size The size in bytes associated with status.
                unformatted: the maximum achievable size of the media
                formatted:   the currently formatted capacity
                unknown:     maximum capacity of drive or of media
    @param bl_sas Additional info "Block Length/Spare Area Size".
                  Expected to be constantly 2048 for non-BD media.
    @param num_formats The number of available formats. To be used with
                       burn_disc_get_format_descr() to obtain such a format
                       and eventually with burn_disc_format() to select one.
    @return 1 reply is valid , <=0 failure
*/
int burn_disc_get_formats(struct burn_drive *drive, int *status, off_t *size,
				unsigned *bl_sas, int *num_formats);

/** Inquire parameters of an available media format.
    @param drive The drive with the disc to format.
    @param index The index of the format item. Beginning with 0 up to reply
                 parameter from burn_disc_get_formats() : num_formats - 1
    @param type  The format type.  See mmc5r03c.pdf, 6.5, 04h FORMAT UNIT.
                 0x00=full, 0x10=CD-RW/DVD-RW full, 0x11=CD-RW/DVD-RW grow,
                 0x15=DVD-RW quick, 0x13=DVD-RW quick grow,
                 0x26=DVD+RW background
    @param size  The maximum size in bytes achievable with this format.
    @param tdp   Type Dependent Parameter. See mmc5r03c.pdf.
    @return 1 reply is valid , <=0 failure
*/
int burn_disc_get_format_descr(struct burn_drive *drive, int index,
				int *type, off_t *size, unsigned *tdp);



/* ts A61109 : this was and is defunct */
/** Read a disc from the drive and write it to an fd pair. The drive must be
    grabbed successfully BEFORE calling this function. Always ensure that the
    drive reports a status of BURN_DISC_FULL before calling this function.
    @param drive The drive from which to read a disc.
    @param o The options for the read operation.
*/
void burn_disc_read(struct burn_drive *drive, const struct burn_read_opts *o);


/* ts A70219 */
/** Examines a completed setup for burn_disc_write() wether it is permissible
    with drive and media. This function is called by burn_disc_write() but
    an application might be interested in this check in advance.
    @param o The options for the writing operation.
    @param disc The descrition of the disc to be created
    @param reasons Eventually returns a list of rejection reason statements
    @param silent 1= do not issue error messages , 0= report problems
    @return 1 ok, -1= no recordable media detected, 0= other failure
*/
int burn_precheck_write(struct burn_write_opts *o, struct burn_disc *disc,
                        char reasons[1024], int silent);

/* <<< enabling switch for internal usage and trust in this function */
#define Libburn_precheck_write_ruleS 1


/** Write a disc in the drive. The drive must be grabbed successfully before
    calling this function. Always ensure that the drive reports a status of
    BURN_DISC_BLANK before calling this function.
    Note: write_type BURN_WRITE_SAO is currently not capable of writing a mix
    of data and audio tracks. You must use BURN_WRITE_TAO for such sessions.
    To be set by burn_write_opts_set_write_type(). 
    @param o The options for the writing operation.
    @param disc The struct burn_disc * that described the disc to be created
*/
void burn_disc_write(struct burn_write_opts *o, struct burn_disc *disc);

/** Cancel an operation on a drive.
    This will only work when the drive's busy state is BURN_DRIVE_READING or
    BURN_DRIVE_WRITING.
    @param drive The drive on which to cancel the current operation.
*/
void burn_drive_cancel(struct burn_drive *drive);


/* ts A61223 */
/** Inquire wether the most recent write run was successful. Reasons for
    non-success may be: rejection of burn parameters, abort during fatal errors
    during write, a call to burn_drive_cancel() by the application thread.
    @param d The drive to inquire.
    @return 1=burn seems to have went well, 0=burn failed 
*/
int burn_drive_wrote_well(struct burn_drive *d);


/** Convert a minute-second-frame (MSF) value to sector count
    @param m Minute component
    @param s Second component
    @param f Frame component
    @return The sector count
*/
int burn_msf_to_sectors(int m, int s, int f);

/** Convert a sector count to minute-second-frame (MSF)
    @param sectors The sector count
    @param m Returns the minute component
    @param s Returns the second component
    @param f Returns the frame component
*/
void burn_sectors_to_msf(int sectors, int *m, int *s, int *f);

/** Convert a minute-second-frame (MSF) value to an lba
    @param m Minute component
    @param s Second component
    @param f Frame component
    @return The lba
*/
int burn_msf_to_lba(int m, int s, int f);

/** Convert an lba to minute-second-frame (MSF)
    @param lba The lba
    @param m Returns the minute component
    @param s Returns the second component
    @param f Returns the frame component
*/
void burn_lba_to_msf(int lba, int *m, int *s, int *f);

/** Create a new disc */
struct burn_disc *burn_disc_create(void);

/** Delete disc and decrease the reference count on all its sessions
	@param d The disc to be freed
*/
void burn_disc_free(struct burn_disc *d);

/** Create a new session */
struct burn_session *burn_session_create(void);

/** Free a session (and decrease reference count on all tracks inside)
	@param s Session to be freed
*/
void burn_session_free(struct burn_session *s);

/** Add a session to a disc at a specific position, increasing the 
    sessions's reference count.
	@param d Disc to add the session to
	@param s Session to add to the disc
	@param pos position to add at (BURN_POS_END is "at the end")
	@return 0 for failure, 1 for success
*/
int burn_disc_add_session(struct burn_disc *d, struct burn_session *s,
			  unsigned int pos);

/** Remove a session from a disc
	@param d Disc to remove session from
	@param s Session pointer to find and remove
*/
int burn_disc_remove_session(struct burn_disc *d, struct burn_session *s);


/** Create a track (for TAO recording, or to put in a session) */
struct burn_track *burn_track_create(void);

/** Free a track
	@param t Track to free
*/
void burn_track_free(struct burn_track *t);

/** Add a track to a session at specified position
	@param s Session to add to
	@param t Track to insert in session
	@param pos position to add at (BURN_POS_END is "at the end")
	@return 0 for failure, 1 for success
*/
int burn_session_add_track(struct burn_session *s, struct burn_track *t,
			   unsigned int pos);

/** Remove a track from a session
	@param s Session to remove track from
	@param t Track pointer to find and remove
	@return 0 for failure, 1 for success
*/
int burn_session_remove_track(struct burn_session *s, struct burn_track *t);


/** Define the data in a track
	@param t the track to define
	@param offset The lib will write this many 0s before start of data
	@param tail The number of extra 0s to write after data
	@param pad 1 means the lib should pad the last sector with 0s if the
	       track isn't exactly sector sized.  (otherwise the lib will
	       begin reading from the next track)
	@param mode data format (bitfield)
*/
void burn_track_define_data(struct burn_track *t, int offset, int tail,
			    int pad, int mode);


/* ts A61024 */
/** Define wether a track shall swap bytes of its input stream.
    @param t The track to change
    @param swap_source_bytes 0=do not swap, 1=swap byte pairs
    @return 1=success , 0=unacceptable value
*/
int burn_track_set_byte_swap(struct burn_track *t, int swap_source_bytes);


/** Set the ISRC details for a track
	@param t The track to change
	@param country the 2 char country code. Each character must be
	       only numbers or letters.
	@param owner 3 char owner code. Each character must be only numbers
	       or letters.
	@param year 2 digit year. A number in 0-99 (Yep, not Y2K friendly).
	@param serial 5 digit serial number. A number in 0-99999.
*/
void burn_track_set_isrc(struct burn_track *t, char *country, char *owner,
			 unsigned char year, unsigned int serial);

/** Disable ISRC parameters for a track
	@param t The track to change
*/
void burn_track_clear_isrc(struct burn_track *t);

/** Hide the first track in the "pre gap" of the disc
	@param s session to change
	@param onoff 1 to enable hiding, 0 to disable
*/
void burn_session_hide_first_track(struct burn_session *s, int onoff);

/** Get the drive's disc struct - free when done
	@param d drive to query
	@return the disc struct or NULL on failure
*/
struct burn_disc *burn_drive_get_disc(struct burn_drive *d);

/** Set the track's data source
	@param t The track to set the data source for
	@param s The data source to use for the contents of the track
	@return An error code stating if the source is ready for use for
	        writing the track, or if an error occured
    
*/
enum burn_source_status burn_track_set_source(struct burn_track *t,
					      struct burn_source *s);


/* ts A70218 */
/** Set a default track size to be used only if the track turns out to be of
    unpredictable length and if the effective write type demands a fixed size.
    This can be useful to enable write types CD SAO or DVD DAO together with
    a track source like stdin. If the track source delivers fewer bytes than
    announced then the track will be padded up with zeros.
    @param t The track to change
    @param size The size to set
    @return 0=failure 1=sucess
*/
int burn_track_set_default_size(struct burn_track *t, off_t size);


/** Free a burn_source (decrease its refcount and maybe free it)
	@param s Source to free
*/
void burn_source_free(struct burn_source *s);

/** Creates a data source for an image file (and maybe subcode file) */
struct burn_source *burn_file_source_new(const char *path,
					 const char *subpath);

/** Creates a data source for an image file (a track) from an open
    readable filedescriptor, an eventually open readable subcodes file
    descriptor and eventually a fixed size in bytes.
    @param datafd The source of data.
    @param subfd The eventual source for subcodes. Not used if -1.
    @param size The eventual fixed size of eventually both fds. 
                If this value is 0, the size will be determined from datafd.
*/
struct burn_source *burn_fd_source_new(int datafd, int subfd, off_t size);


/** Tells how long a track will be on disc
    >>> NOTE: Not reliable with tracks of undefined length
*/
int burn_track_get_sectors(struct burn_track *);


/* ts A61101 */
/** Tells how many source bytes have been read and how many data bytes have
    been written by the track during burn */
int burn_track_get_counters(struct burn_track *t, 
                            off_t *read_bytes, off_t *written_bytes);


/** Sets drive read and write speed
    @param d The drive to set speed for
    @param read Read speed in k/s (0 is max)
    @param write Write speed in k/s (0 is max)
*/
void burn_drive_set_speed(struct burn_drive *d, int read, int write);

/* these are for my debugging, they will disappear */
void burn_structure_print_disc(struct burn_disc *d);
void burn_structure_print_session(struct burn_session *s);
void burn_structure_print_track(struct burn_track *t);

/** Sets the write type for the write_opts struct.
    Note: write_type BURN_WRITE_SAO is currently not capable of writing a mix
    of data and audio tracks. You must use BURN_WRITE_TAO for such sessions.
    @param opts The write opts to change
    @param write_type The write type to use
    @param block_type The block type to use
    @return Returns 1 on success and 0 on failure.
*/
int burn_write_opts_set_write_type(struct burn_write_opts *opts,
				   enum burn_write_types write_type,
				   int block_type);

/* ts A70207 */
/** As an alternative to burn_write_opts_set_write_type() this function tries
    to find a suitable write type and block type for a given write job
    described by opts and disc. To be used after all other setups have been
    made, i.e. immediately before burn_disc_write().
    @param opts The nearly complete write opts to change
    @param disc The already composed session and track model
    @param reasons This text string collects reasons for decision resp. failure
    @param flag Bitfield for control purposes:
                bit0= do not choose type but check the one that is already set
                bit1= do not issue error messages via burn_msgs queue
                      (is automatically set with bit0)
    @return Chosen write type. BURN_WRITE_NONE on failure.
*/
enum burn_write_types burn_write_opts_auto_write_type(
          struct burn_write_opts *opts, struct burn_disc *disc,
          char reasons[1024], int flag);


/** Supplies toc entries for writing - not normally required for cd mastering
    @param opts The write opts to change
    @param count The number of entries
    @param toc_entries
*/
void burn_write_opts_set_toc_entries(struct burn_write_opts *opts,
				     int count,
				     struct burn_toc_entry *toc_entries);

/** Sets the session format for a disc
    @param opts The write opts to change
    @param format The session format to set
*/
void burn_write_opts_set_format(struct burn_write_opts *opts, int format);

/** Sets the simulate value for the write_opts struct
    @param opts The write opts to change
    @param sim If non-zero, the drive will perform a simulation instead of a burn
    @return Returns 1 on success and 0 on failure.
*/
int  burn_write_opts_set_simulate(struct burn_write_opts *opts, int sim);

/** Controls buffer underrun prevention
    @param opts The write opts to change
    @param underrun_proof if non-zero, buffer underrun protection is enabled
    @return Returns 1 on success and 0 on failure.
*/
int burn_write_opts_set_underrun_proof(struct burn_write_opts *opts,
				       int underrun_proof);

/** Sets whether to use opc or not with the write_opts struct
    @param opts The write opts to change
    @param opc If non-zero, optical power calibration will be performed at
               start of burn
	 
*/
void burn_write_opts_set_perform_opc(struct burn_write_opts *opts, int opc);

void burn_write_opts_set_has_mediacatalog(struct burn_write_opts *opts, int has_mediacatalog);

void burn_write_opts_set_mediacatalog(struct burn_write_opts *opts, unsigned char mediacatalog[13]);


/* ts A61106 */
/** Sets the multi flag which eventually marks the emerging session as not
    being the last one and thus creating a BURN_DISC_APPENDABLE media.
    @param multi 1=media will be appendable, 0=media will be closed (default) 
*/
void burn_write_opts_set_multi(struct burn_write_opts *opts, int multi);


/* ts A61222 */
/** Sets a start address for writing to media and write modes which allow to
    choose this address at all (DVD+RW, DVD-RAM, formatted DVD-RW only for
    now). The address is given in bytes. If it is not -1 then a write run
    will fail if choice of start address is not supported or if the block
    alignment of the address is not suitable for media and write mode.
    (Alignment to 32 kB blocks is advised with DVD media.)
    @param opts The write opts to change
    @param value The address in bytes (-1 = start at default address)
*/
void burn_write_opts_set_start_byte(struct burn_write_opts *opts, off_t value);


/* ts A70213 */
/** Caution: still immature and likely to change. Problems arose with
    sequential DVD-RW on one drive.

    Controls wether the whole available space of the media shall be filled up
    by the last track of the last session.
    @param opts The write opts to change
    @param fill_up_media If 1 : fill up by last track, if 0 = do not fill up
*/
void burn_write_opts_set_fillup(struct burn_write_opts *opts,
                                int fill_up_media);



/** Sets whether to read in raw mode or not
    @param opts The read opts to change
    @param raw_mode If non-zero, reading will be done in raw mode, so that everything in the data tracks on the
            disc is read, including headers.
*/
void burn_read_opts_set_raw(struct burn_read_opts *opts, int raw_mode);

/** Sets whether to report c2 errors or not 
    @param opts The read opts to change
    @param c2errors If non-zero, report c2 errors.
*/
void burn_read_opts_set_c2errors(struct burn_read_opts *opts, int c2errors);

/** Sets whether to read subcodes from audio tracks or not
    @param opts The read opts to change
    @param subcodes_audio If non-zero, read subcodes from audio tracks on the disc.
*/
void burn_read_opts_read_subcodes_audio(struct burn_read_opts *opts,
					int subcodes_audio);

/** Sets whether to read subcodes from data tracks or not 
    @param opts The read opts to change
    @param subcodes_data If non-zero, read subcodes from data tracks on the disc.
*/
void burn_read_opts_read_subcodes_data(struct burn_read_opts *opts,
				       int subcodes_data);

/** Sets whether to recover errors if possible
    @param opts The read opts to change
    @param hardware_error_recovery If non-zero, attempt to recover errors if possible.
*/
void burn_read_opts_set_hardware_error_recovery(struct burn_read_opts *opts,
						int hardware_error_recovery);

/** Sets whether to report recovered errors or not
    @param opts The read opts to change
    @param report_recovered_errors If non-zero, recovered errors will be reported.
*/
void burn_read_opts_report_recovered_errors(struct burn_read_opts *opts,
					    int report_recovered_errors);

/** Sets whether blocks with unrecoverable errors should be read or not
    @param opts The read opts to change
    @param transfer_damaged_blocks If non-zero, blocks with unrecoverable errors will still be read.
*/
void burn_read_opts_transfer_damaged_blocks(struct burn_read_opts *opts,
					    int transfer_damaged_blocks);

/** Sets the number of retries to attempt when trying to correct an error
    @param opts The read opts to change
    @param hardware_error_retries The number of retries to attempt when correcting an error.
*/
void burn_read_opts_set_hardware_error_retries(struct burn_read_opts *opts,
					       unsigned char hardware_error_retries);

/** Gets the maximum write speed for a drive and eventually loaded media.
    The return value might change by the media type of already loaded media,
    again by call burn_drive_grab() and again by call burn_disc_read_atip(). 
    @param d Drive to query
    @return Maximum write speed in K/s
*/
int burn_drive_get_write_speed(struct burn_drive *d);


/* ts A61021 */
/** Gets the minimum write speed for a drive and eventually loaded media.
    The return value might change by the media type of already loaded media, 
    again by call burn_drive_grab() and again by call burn_disc_read_atip().
    @param d Drive to query
    @return Minimum write speed in K/s
*/
int burn_drive_get_min_write_speed(struct burn_drive *d);


/** Gets the maximum read speed for a drive
    @param d Drive to query
    @return Maximum read speed in K/s
*/
int burn_drive_get_read_speed(struct burn_drive *d);


/* ts A61226 */
/** Obtain a copy of the current speed descriptor list. The drive's list gets
    updated on various occasions such as burn_drive_grab() but the copy
    obtained here stays untouched. It has to be disposed via
    burn_drive_free_speedlist() when it is not longer needed. Speeds
    may appear several times in the list. The list content depends much on
    drive and media type. It seems that .source == 1 applies mostly to CD media
    whereas .source == 2 applies to any media.
    @param d Drive to query
    @param speed_list The copy. If empty, *speed_list gets returned as NULL.
    @return 1=success , 0=list empty , <0 severe error
*/
int burn_drive_get_speedlist(struct burn_drive *d,
                             struct burn_speed_descriptor **speed_list);

/* ts A61226 */
/** Dispose a speed descriptor list copy which was obtained by
    burn_drive_get_speedlist().
    @param speed_list The list copy. *speed_list gets set to NULL.
    @return 1=list disposed , 0= *speedlist was already NULL
*/
int burn_drive_free_speedlist(struct burn_speed_descriptor **speed_list);


/* ts A70203 */
/** The reply structure for burn_disc_get_multi_caps()
*/
struct burn_multi_caps {

	/* Multi-session capability allows to keep the media appendable after
	   writing a session. It also guarantees that the drive will be able
	   to predict and use the appropriate Next Writeable Address to place
	   the next session on the media without overwriting the existing ones.
	   It does not guarantee that the selected write type is able to do
	   an appending session after the next session. (E.g. CD SAO is capable
	   of multi-session by keeping a disc appendable. But .might_do_sao
	   will be 0 afterwards, when checking the appendable media.)
	    1= media may be kept appendable by burn_write_opts_set_multi(o,1)
 	    0= media will not be appendable
	*/
	int multi_session;

	/* Multi-track capability allows to write more than one track source
	   during a single session. The written tracks can later be found in
	   libburn's TOC model with their start addresses and sizes.
	    1= multiple tracks per session are allowed
	    0= only one track per session allowed
	*/
	int multi_track;

	/* Start-address capability allows to set a non-zero address with
	   burn_write_opts_set_start_byte(). Eventually this has to respect
	   .start_alignment and .start_range_low, .start_range_high in this
	   structure.
	    1= non-zero start address is allowed
            0= only start address 0 is allowed (to depict the drive's own idea
               about the appropriate write start)
	*/
	int start_adr;

	/** The alignment for start addresses.
	    ( start_address % start_alignment ) must be 0.
	*/
	off_t start_alignment;

	/** The lowest permissible start address.
	*/
	off_t start_range_low;

	/** The highest addressable start address.
	*/
	off_t start_range_high;

	/** Potential availability of write modes
	     3= allowed but not to be chosen automatically
  	     2= available, no size prediction necessary
	     1= available, needs exact size prediction
	     0= not available
	    With CD media (profiles 0x09 and 0x0a) check also the elements
	    *_block_types of the according write mode.
	*/
	int might_do_tao;
	int might_do_sao;
	int might_do_raw;

	/** Generally advised write mode.
	    Not necessarily the one chosen by burn_write_opts_auto_write_type()
	    because the burn_disc structure might impose particular demands.
	*/
	enum burn_write_types advised_write_mode;

	/** Write mode as given by parameter wt of burn_disc_get_multi_caps().
	*/
	enum burn_write_types selected_write_mode;

	/** Profile number which was current when the reply was generated */
	int current_profile;

	/** Wether the current profile indicates CD media. 1=yes, 0=no */
	int current_is_cd_profile;
};

/** Allocates a struct burn_multi_caps (see above) and fills it with values
    which are appropriate for the drive and the loaded media. The drive
    must be grabbed for this call. The returned structure has to be disposed
    via burn_disc_free_multi_caps() when no longer needed.
    @param d The drive to inquire
    @param wt With BURN_WRITE_NONE the best capabilities of all write modes
              get returned. If set to a write mode like BURN_WRITE_SAO the
              capabilities with that particular mode are returned and the
              return value is 0 if the desired mode is not possible.
    @param caps returns the info structure
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return < 0 : error , 0 : writing seems impossible , 1 : writing possible 
*/
int burn_disc_get_multi_caps(struct burn_drive *d, enum burn_write_types wt,
			 struct burn_multi_caps **caps, int flag);

/** Removes from memory a multi session info structure which was returned by
    burn_disc_get_multi_caps(). The pointer *caps gets set to NULL.
    @param caps the info structure to dispose (note: pointer to pointer)
    @return 0 : *caps was already NULL, 1 : memory object was disposed
*/
int burn_disc_free_multi_caps(struct burn_multi_caps **caps);


/** Gets a copy of the toc_entry structure associated with a track
    @param t Track to get the entry from
    @param entry Struct for the library to fill out
*/
void burn_track_get_entry(struct burn_track *t, struct burn_toc_entry *entry);

/** Gets a copy of the toc_entry structure associated with a session's lead out
    @param s Session to get the entry from
    @param entry Struct for the library to fill out
*/
void burn_session_get_leadout_entry(struct burn_session *s,
                                    struct burn_toc_entry *entry);

/** Gets an array of all the sessions for the disc
    THIS IS NO LONGER VALID AFTER YOU ADD OR REMOVE A SESSION
    @param d Disc to get session array for
    @param num Returns the number of sessions in the array
    @return array of sessions
*/
struct burn_session **burn_disc_get_sessions(struct burn_disc *d,
                                             int *num);

int burn_disc_get_sectors(struct burn_disc *d);

/** Gets an array of all the tracks for a session
    THIS IS NO LONGER VALID AFTER YOU ADD OR REMOVE A TRACK
    @param s session to get track array for
    @param num Returns the number of tracks in the array
    @return array of tracks
*/
struct burn_track **burn_session_get_tracks(struct burn_session *s,
                                            int *num);

int burn_session_get_sectors(struct burn_session *s);

/** Gets the mode of a track
    @param track the track to query
    @return the track's mode
*/
int burn_track_get_mode(struct burn_track *track);

/** Returns whether the first track of a session is hidden in the pregap
    @param session the session to query
    @return non-zero means the first track is hidden
*/
int burn_session_get_hidefirst(struct burn_session *session);

/** Returns the library's version in its parts
    @param major The major version number
    @param minor The minor version number
    @param micro The micro version number
*/
void burn_version(int *major, int *minor, int *micro);


/* ts A60924 : ticket 74 */
/** Control queueing and stderr printing of messages from libburn.
    Severity may be one of "NEVER", "FATAL", "SORRY", "WARNING", "HINT",
    "NOTE", "UPDATE", "DEBUG", "ALL".
    @param queue_severity Gives the minimum limit for messages to be queued.
                          Default: "NEVER". If you queue messages then you
                          must consume them by burn_msgs_obtain().
    @param print_severity Does the same for messages to be printed directly
                          to stderr. Default: "FATAL".
    @param print_id       A text prefix to be printed before the message.
    @return               >0 for success, <=0 for error

*/
int burn_msgs_set_severities(char *queue_severity,
                             char *print_severity, char *print_id);

/* ts A60924 : ticket 74 */
#define BURN_MSGS_MESSAGE_LEN 4096

/** Obtain the oldest pending libburn message from the queue which has at
    least the given minimum_severity. This message and any older message of
    lower severity will get discarded from the queue and is then lost forever.
    @param minimum_severity  may be one of "NEVER", "FATAL", "SORRY",
                      "WARNING", "HINT", "NOTE", "UPDATE", "DEBUG", "ALL".
                      To call with minimum_severity "NEVER" will discard the
                      whole queue.
    @param error_code Will become a unique error code as liste in
                      libburn/libdax_msgs.h
    @param msg_text   Must provide at least BURN_MSGS_MESSAGE_LEN bytes.
    @param os_errno   Will become the eventual errno related to the message
    @param severity   Will become the severity related to the message and
                      should provide at least 80 bytes.
    @return 1 if a matching item was found, 0 if not, <0 for severe errors
*/
int burn_msgs_obtain(char *minimum_severity,
                     int *error_code, char msg_text[], int *os_errno,
                     char severity[]);


/* ts A61002 */
/* The prototype of a handler function suitable for burn_set_abort_handling().
   Such a function has to return -2 if it does not want the process to
   exit with value 1.
*/
typedef int (*burn_abort_handler_t)(void *handle, int signum, int flag);

/** Control builtin signal handling. See also burn_abort().
    @param handle Opaque handle eventually pointing to an application
                  provided memory object
    @param handler A function to be called on signals. It will get handle as
                  argument. It should finally call burn_abort(). See there.
    @param mode : 0 call handler(handle, signum, 0) on nearly all signals
                  1 enable system default reaction on all signals
                  2 try to ignore nearly all signals
                 10 like mode 2 but handle SIGABRT like with mode 0
    Arguments (text, NULL, 0) activate the builtin abort handler. It will
    eventually call burn_abort() and then perform exit(1). If text is not NULL
    then it is used as prefix for pacifier messages of burn_abort_pacifier().
*/
void burn_set_signal_handling(void *handle, burn_abort_handler_t handler, 
			     int mode);

#ifndef DOXYGEN

BURN_END_DECLS

#endif

#endif /*LIBBURN_H*/
