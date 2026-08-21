/*==============================================================================

 Copyright (c) 2018 - 2026 by Anton Kholomiov.
 For more information visit www.rabiensoftware.com

 ==============================================================================*/


/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.


 BEGIN_JUCE_MODULE_DECLARATION

  ID:                   juce_csd
  vendor:               Anton Kholomiov
  version:              0.0.1
  name:                 Csound for JUCE Utilities
  description:          Csound for JUCE Utilities
  website:              www.csound-for-juce.com
  license:              BSD
  minimumCppStandard:   20

  dependencies:         juce_core juce_data_structures
  OSXFrameworks:        Security

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#pragma once

namespace juce_csd {
  #include "audio/FxProcessor.h"
  #include "params/Parameters.h"
  #include "plugin/FxPluginProcessor.h"
}
