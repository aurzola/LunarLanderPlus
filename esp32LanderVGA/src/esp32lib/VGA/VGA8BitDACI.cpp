/*
	Author: Martin-Laclaustra 2021
	License: 
	Creative Commons Attribution ShareAlike 4.0
	https://creativecommons.org/licenses/by-sa/4.0/
	
	For further details check out: 
		https://github.com/bitluni
*/

#include "VGA8BitDACI.h"

// --- Lunar Lander fork: map the per-line row pointers into the sketch-owned
// 320x240 frame store instead of malloc'ing a 640x480 (300 KB) framebuffer.
// For MODE640x480 (vDiv=1, vRes=480) each output line maps to shadow line
// y>>1 (2x vertical scale, done here so interruptPixelLine can stay simple);
// for MODE320x240 (vDiv=2, vRes=480 -> yres=240) the mapping is 1:1.
uint8_t **VGA8BitDACI::allocateFrameBuffer()
{
	uint8_t **rows = (uint8_t **)malloc(yres * sizeof(uint8_t *));
	if (!rows)
		ERROR("Not enough memory for frame buffer rows");
	const int vScale = (mode.vDiv == 1) ? 2 : 1;
	for (int y = 0; y < yres; y++)
		rows[y] = &frameStore_[(y / vScale) * frameStoreW_];
	return rows;
}

void IRAM_ATTR VGA8BitDACI::interrupt(void *arg)
{
	VGA8BitDACI * staticthis = (VGA8BitDACI *)arg;

	//obtain currently rendered line from the buffer just read, based on the conventioned ordering and buffers per line
	staticthis->currentLine = staticthis->dmaBufferDescriptorActive >> ( (staticthis->descriptorsPerLine==2) ? 1 : 0 );

	//in the case of two buffers per line,
	//render only when the sync half of the line ended (longer period until next interrupt)
	//else exit early
	//This might need to be revised, because it might be better to overlap and miss the second interrupt
	if ( (staticthis->descriptorsPerLine==2) && ((staticthis->dmaBufferDescriptorActive & 1) != 0) ) return;

	//TO DO: This should be precalculated outside the interrupt
	int vInactiveLinesCount = staticthis->mode.vFront + staticthis->mode.vSync + staticthis->mode.vBack;

	//render ahead (the lenght of buffered lines)
	int renderLine = (staticthis->currentLine + staticthis->lineBufferCount);
	if (renderLine >= staticthis->totalLines) renderLine -= staticthis->totalLines;

	if (renderLine >= vInactiveLinesCount)
	{
		int renderActiveLine = renderLine - vInactiveLinesCount;
		uint8_t *activeRenderingBuffer = ((uint8_t *)
		staticthis->dmaBufferDescriptors[staticthis->indexRendererDataBuffer[0] + renderActiveLine * staticthis->descriptorsPerLine + staticthis->descriptorsPerLine - 1].buffer() + staticthis->dataOffsetInLineInBytes
		);

		int y = renderActiveLine / staticthis->mode.vDiv;
		if (y >= 0 && y < staticthis->yres)
			staticthis->interruptPixelLine(y, activeRenderingBuffer, arg);
	}

	if (renderLine == 0)
		staticthis->vSyncPassed = true;
}

	//LOWER LIMIT: THE CODE BETWEEN THESE MARKS IS SHARED BETWEEN 3BIT, 6BIT, AND 14BIT

void IRAM_ATTR VGA8BitDACI::interruptPixelLine(int y, uint8_t *pixels, void *arg)
{
	VGA8BitDACI * staticthis = (VGA8BitDACI *)arg;
	unsigned long syncBits = (staticthis->hsyncBitI | staticthis->vsyncBitI) * staticthis->rendererStaticReplicate32mask;
	uint8_t *line = staticthis->frontBuffer[y];
	if (staticthis->mode.hRes == 2 * staticthis->frameStoreW_)
	{
		// Lunar Lander fork, MODE640x480: 2x horizontal scale. Each 16-bit DAC
		// sample takes its pixel from the MSByte; sample pairs map to the two
		// halves of each 32-bit word. Both samples of a word repeat the same
		// shadow pixel, doubling the width.
		uint32_t *out = (uint32_t *)pixels;
		for (int i = 0; i < staticthis->mode.hRes / 2; i++)
		{
			unsigned long c = staticthis->colorDepthConversionFactor * (int)line[i];
			out[i] = syncBits | (c & 0xff00) | ((c & 0xff00) << 16);
		}
	}
	else
	{
		for (int i = 0; i < staticthis->mode.hRes / 2; i++)
		{
			//writing two pixels improves speed drastically (avoids memory reads)
			//values must be shifted to the MSByte to be output
			//which is equivalent to multiplying by 256
			//instead of shifting, colorDepthConversionFactor was not divided by 256
			((uint32_t *)pixels)[i] = syncBits |
				((staticthis->colorDepthConversionFactor*(int)line[i * 2 + 1]) & 0xff00)
				| (((staticthis->colorDepthConversionFactor*(int)line[i * 2]) & 0xff00) << 16);
		}
	}
}

