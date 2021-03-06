#!/bin/sh
# Export a jail into a self-contained file for transport / backup
######################################################################

# Source our functions
PROGDIR="/usr/local/share/warden"

# Source our variables
. ${PROGDIR}/scripts/backend/functions.sh

EXPORTNAME="$1"
JAILNAME="$1"
OUTDIR="$2"

if [ -z "${OUTDIR}" ]; then OUTDIR="$WTMP" ; fi

if [ -z "${EXPORTNAME}" ]
then
  echo "ERROR: No jail specified to export!"
  exit 5
fi

if [ -z "${JDIR}" ]
then
  echo "ERROR: JDIR is unset!!!!"
  exit 5
fi

JAILDIR="${JDIR}/${EXPORTNAME}"

if [ ! -d "${JAILDIR}" ]
then
  echo "ERROR: No jail located at ${JAILDIR}"
  exit 5
fi

set_warden_metadir

# First check if this jail is running, and stop it
echo "Checking jail status..."
${PROGDIR}/scripts/backend/checkstatus.sh "${EXPORTNAME}"
if [ "$?" = "0" ]
then
  echo "Stopping jail for export..."
  ${PROGDIR}/scripts/backend/stopjail.sh "${EXPORTNAME}"
fi

# Reset JAILDIR
JAILNAME="$1"
JAILDIR="${JDIR}/${EXPORTNAME}"
set_warden_metadir

# Now that the jail is stopped, lets make a large tbz file of it
cd ${JAILDIR}

# Get the Hostname
HOST="`cat ${JMETADIR}/host`"

IP4="`cat ${JMETADIR}/ipv4 2>/dev/null`"
IP6="`cat ${JMETADIR}/ipv6 2>/dev/null`"

get_ip_and_netmask "${IP4}"
IP4="${JIP}"
MASK4="${JMASK}"

get_ip_and_netmask "${IP6}"
IP6="${JIP}"
MASK6="${JMASK}"

echo "Creating compressed archive of ${EXPORTNAME}... Please Wait..."
tar cvJf "${WTMP}/${EXPORTNAME}.txz" -C "${JAILDIR}" . 2>${WTMP}/${EXPORTNAME}.files

cd ${WTMP}

echo "Creating jail metadata..."
LINES="`wc -l ${EXPORTNAME}.files | sed -e 's, ,,g' | cut -d '.' -f 1`"

# Finished, now make the header info
cd ${WTMP}
echo "[Warden file]
Ver: 1.0 
OS: `uname -r | cut -d '-' -f 1`
Files: $LINES
IP4: ${IP4}/${MASK4}
IP6: ${IP6}/${MASK6}
HOST: ${HOST}
" >${WTMP}/${EXPORTNAME}.header

# Copy over jail extra meta-data
cp ${JMETADIR}/jail-* ${WTMP}/ 2>/dev/null

# Compress the header file
tar cvzf ${EXPORTNAME}.header.tgz ${EXPORTNAME}.header jail-* 2>/dev/null

# Create our spacer
echo "
___WARDEN_START___" > .spacer

# Make the .wdn file now
cat ${EXPORTNAME}.header.tgz .spacer ${EXPORTNAME}.txz > ${EXPORTNAME}.wdn

# Remove the old files
rm ${EXPORTNAME}.header
rm ${EXPORTNAME}.files
rm ${EXPORTNAME}.txz
rm .spacer
rm ${EXPORTNAME}.header.tgz

# Remove any extra jail meta-files from WTMP
for i in `ls ${JMETADIR}/jail-* 2>/dev/null`
do
  mFile=`basename $i`
  rm $mFile
done

if [ ! -d "$OUTDIR" ] ; then
  mkdir -p ${OUTDIR}
fi
if [ "$OUTDIR" != "$WTMP" ] ; then
  mv ${EXPORTNAME}.wdn ${OUTDIR}/
fi
echo "Created ${EXPORTNAME}.wdn in ${OUTDIR}" >&1

exit 0
