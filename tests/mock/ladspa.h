#pragma once
/* Test-only LADSPA ABI declarations, not installed to user's system. */
typedef float LADSPA_Data;
typedef void* LADSPA_Handle;
typedef unsigned long LADSPA_PortDescriptor;
typedef unsigned long LADSPA_PortRangeHintDescriptor;
#define LADSPA_PORT_INPUT 0x1
#define LADSPA_PORT_OUTPUT 0x2
#define LADSPA_PORT_CONTROL 0x4
#define LADSPA_PORT_AUDIO 0x8
#define LADSPA_HINT_BOUNDED_BELOW 0x1
#define LADSPA_HINT_BOUNDED_ABOVE 0x2
#define LADSPA_HINT_TOGGLED 0x4
#define LADSPA_HINT_DEFAULT_1 0x240
#define LADSPA_HINT_DEFAULT_MAXIMUM 0x3c0
#define LADSPA_HINT_DEFAULT_MIDDLE 0x300
struct LADSPA_PortRangeHint { LADSPA_PortRangeHintDescriptor HintDescriptor; LADSPA_Data LowerBound, UpperBound; };
typedef struct LADSPA_PortRangeHint LADSPA_PortRangeHint;
struct LADSPA_Descriptor;
typedef struct LADSPA_Descriptor LADSPA_Descriptor;
struct LADSPA_Descriptor {
  unsigned long UniqueID;
  const char *Label;
  unsigned long Properties;
  const char *Name;
  const char *Maker;
  const char *Copyright;
  unsigned long PortCount;
  const LADSPA_PortDescriptor* PortDescriptors;
  const char* const* PortNames;
  const LADSPA_PortRangeHint* PortRangeHints;
  void* ImplementationData;
  LADSPA_Handle (*instantiate)(const LADSPA_Descriptor*, unsigned long);
  void (*connect_port)(LADSPA_Handle, unsigned long, LADSPA_Data*);
  void (*activate)(LADSPA_Handle);
  void (*run)(LADSPA_Handle, unsigned long);
  void (*run_adding)(LADSPA_Handle, unsigned long);
  void (*set_run_adding_gain)(LADSPA_Handle, LADSPA_Data);
  void (*deactivate)(LADSPA_Handle);
  void (*cleanup)(LADSPA_Handle);
};
