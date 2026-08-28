#!/bin/bash
# usage: ./fetch_test_pair.sh <scenario> [GUID]
# scenario: farend_singletalk | doubletalk | farend_singletalk_with_movement | doubletalk_with_movement
# If GUID is omitted, picks a random one that actually has this scenario available.

SCENARIO=$1
REPO_DIR=~/datasets/AEC-Challenge
SRC_DIR="$REPO_DIR/datasets/real"
DEST_DIR=~/projects/audio_processing/test_data

if [ -z "$2" ]; then
  GUID=$(ls "$SRC_DIR" | grep "_${SCENARIO}_lpb.wav$" | sort -R | head -n 1 | sed "s/_${SCENARIO}_lpb.wav//")
  echo "No GUID given — picked random: $GUID"
else
  GUID=$2
fi

LPB_PATH="datasets/real/${GUID}_${SCENARIO}_lpb.wav"
MIC_PATH="datasets/real/${GUID}_${SCENARIO}_mic.wav"

git -C "$REPO_DIR" lfs pull --include="$LPB_PATH"
git -C "$REPO_DIR" lfs pull --include="$MIC_PATH"

mkdir -p "$DEST_DIR"
cp "$REPO_DIR/$LPB_PATH" "$DEST_DIR/far_end.wav"
cp "$REPO_DIR/$MIC_PATH" "$DEST_DIR/near_end.wav"
echo "Fetched $SCENARIO pair for $GUID into $DEST_DIR"
