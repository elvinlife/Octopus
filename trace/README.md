## Video traces with temporal layers
* The first row records the number of frames in a GoP(group of pictures).
* For the other rows, every row represent one frame:
  * 1st column: the frame id
  * 2nd column: the frame size(in bytes)
  * 3rd column: the SSIM of this frame if decoded(in dB)

## Video traces with quality layers
* The first row records the number of frames in a GoP, and the number of layers in a frame.
* For the other rows, every row represent a layer of one frame:
  * 1st column: the aggregate video bitrate of three layers(in kbps)
  * 2nd column: the frame id
  * 3rd column: the layer id within the frame
  * 4th column: the layer size(in bytes)
  * 5th column: the cumulative video bitrate(in kbps)
  * 6th column: the SSIM of this frame if decoded(in dB)
