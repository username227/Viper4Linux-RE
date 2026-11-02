#!/usr/bin/env python3
"""Pure unit tests; do not connect to PipeWire or touch DSP settings."""
import importlib.util
import pathlib
import unittest

SRC = pathlib.Path(__file__).resolve().parents[1] / 'src/viper_output_graph.py'
spec = importlib.util.spec_from_file_location('viper_output_graph', SRC)
import sys
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


def node(i, name, device_id=None, media='Audio/Sink'):
    props = {'node.name': name, 'node.description': f'Display: {name}', 'media.class': media}
    if device_id is not None:
        props['device.id'] = device_id
    return {'id': i, 'type': 'PipeWire:Interface:Node', 'info': {'props': props}}


def device(i, name):
    return {'id': i, 'type': 'PipeWire:Interface:Device', 'info': {'props': {'device.name': name}}}


def metadata(name):
    return {'id': 10, 'type': 'PipeWire:Interface:Metadata', 'props': {'metadata.name': 'default'},
            'metadata': [{'subject': 0, 'key': 'default.audio.sink', 'value': {'name': name}}]}


def link(i, source, dest):
    return {'id': i, 'type': 'PipeWire:Interface:Link', 'info': {'props': {'link.output.node': source, 'link.input.node': dest}}}


class OutputTests(unittest.TestCase):
    def setUp(self):
        self.graph = module.PipeWireGraph()
        self.graph.update([device(3, 'bluez_card.AB_CD_EF_00_11_22'), device(4, 'alsa_card.pci_0000'),
                           node(33, 'bluez_output.AB_CD_EF.a2dp-sink', 3),
                           node(40, 'alsa_output.pci_0000.analog-stereo', 4),
                           node(50, module.SMART_CAPTURE),
                           node(51, module.SMART_PLAYBACK, media='Stream/Output/Audio')])

    def test_no_substring_matching_and_no_ephemeral_ids(self):
        self.assertEqual(len(self.graph.available()), 2)
        self.graph.update([metadata('bluez_output.AB_CD_EF.a2dp-sink')])
        a = self.graph.active()
        self.assertEqual(a.node_id, 33)
        self.graph.update([{'id': 33}, node(111, 'bluez_output.AB_CD_EF.a2dp-sink', 3)])
        self.assertEqual(self.graph.active().key, a.key)
        self.graph.update([metadata('bluez_output.AB_CD_EF')])
        self.assertIsNone(self.graph.active())

    def test_smart_link_resolves_real_hardware(self):
        self.graph.update([metadata(module.SMART_CAPTURE), link(200, 51, 40), link(201, 51, 40)])
        self.assertEqual(self.graph.active().node_id, 40)
        self.graph.update([link(202, 51, 33)])
        self.assertIsNone(self.graph.active())  # ambiguous temporary routing
        self.graph.update([{'id': 202}, {'id': 200}, {'id': 201}])
        self.assertIsNone(self.graph.active())  # default remains virtual

    def test_missing_device_identity_is_not_remembered(self):
        self.graph.update([node(90, 'ambiguous_output', 99), metadata('ambiguous_output')])
        self.assertIsNone(self.graph.active())

    def test_identically_named_bluetooth_devices_remain_distinct(self):
        # Two real headsets may have the same marketing name but distinct MACs.
        other = node(110, 'bluez_output.B4_E7_B3_65_FE_67.1', 5)
        other['info']['props']['node.description'] = 'EDIFIER NeoBuds Planar'
        first = node(33, 'bluez_output.B4_E7_B3_66_04_8C.1', 3)
        first['info']['props']['node.description'] = 'EDIFIER NeoBuds Planar'
        self.graph.update([device(3, 'bluez_card.B4_E7_B3_66_04_8C'),
                           device(5, 'bluez_card.B4_E7_B3_65_FE_67'), first, other])
        self.assertNotEqual(self.graph.output_for_node(first).key,
                            self.graph.output_for_node(other).key)

    def test_bluetooth_profile_and_node_changes_preserve_identity(self):
        initial = self.graph.output_for_node(self.graph.objects[33])
        self.graph.update([{'id': 33}, node(92, 'bluez_output.AB_CD_EF_00_11_22.a2dp-sink', 3)])
        self.assertEqual(self.graph.output_for_node(self.graph.objects[92]).key,
                         initial.key)
        self.graph.update([{'id': 92}, node(93, 'bluez_output.AB_CD_EF_00_11_22.headset-head-unit', 3)])
        self.assertEqual(self.graph.output_for_node(self.graph.objects[93]).key,
                         initial.key)

    def test_same_card_distinct_alsa_outputs(self):
        speaker = self.graph.output_for_node(self.graph.objects[40])
        headphone_node = node(75, 'alsa_output.pci_0000.Headphones', 4)
        self.graph.update([headphone_node])
        headphones = self.graph.output_for_node(headphone_node)
        self.assertNotEqual(speaker.key, headphones.key)

    def test_multiple_profiles_of_one_headset_appear_once(self):
        self.graph.update([node(92, 'bluez_output.AB_CD_EF_00_11_22.a2dp-sink', 3),
                           node(93, 'bluez_output.AB_CD_EF_00_11_22.headset-head-unit', 3)])
        self.assertEqual(len(self.graph.available()), 2)  # headset + internal speakers

    def test_ambiguous_bluetooth_card_does_not_store_identity(self):
        self.graph.update([device(14, 'bluez_card.Unknown'),
                           node(99, 'bluez_output.Uncertain', 14)])
        self.assertIsNone(self.graph.output_for_node(self.graph.objects[99]))

    def test_stream_parser_handles_nested_json_and_chunks(self):
        stream = module.JsonArrayStream()
        payload = b'[ {"id":1,"type":"PipeWire:Interface:Node","info":{"props":{"node.description":"A ] \\\" B"}}} ]\n[ {"id":1} ]'
        result = []
        for chunk in [payload[:8], payload[8:27], payload[27:70], payload[70:]]:
            result.extend(stream.feed(chunk))
        self.assertEqual(len(result), 2)
        self.assertEqual(result[1], [{'id': 1}])

    def test_metadata_string_value(self):
        m = metadata('alsa_output.pci_0000.analog-stereo')
        m['metadata'][0]['value'] = '{"name":"alsa_output.pci_0000.analog-stereo"}'
        self.graph.update([m])
        self.assertEqual(self.graph.active().node_id, 40)


if __name__ == '__main__':
    unittest.main()
