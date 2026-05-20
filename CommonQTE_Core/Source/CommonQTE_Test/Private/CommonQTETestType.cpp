#include "CommonQTETestType.h"

FCommonQTETestVisualizationConfig::FCommonQTETestVisualizationConfig() = default;

FCommonQTETestConfig::FCommonQTETestConfig()
{
	FixedCellContents = {1, 2};
	PresentationDrainConfig.MaxMessagesPerDrain = 100;
	PresentationDrainConfig.DrainInterval = 0.0f;
	PresentationDrainConfig.bAutoDrain = true;
}
