#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"  

namespace blender::nodes::node_shader_kevin_node_cc {

  static void node_shader_init_kevin_node(bNodeTree* UNUSED(ntree), bNode* node)
  {
    KevinNodeData* data = MEM_cnew<KevinNodeData>(__func__);
    data->custom_data1 = 10;
    data->custom_data2 = 20;
    node->storage = data;
  }

  static void node_shader_buts_kevinnode(uiLayout* layout, bContext* UNUSED(C), PointerRNA* ptr)
  {
    uiItemR(layout, ptr, "custom_data1", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
    uiItemR(layout, ptr, "custom_data2", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  }

} // namespace blender::nodes::node_shader_kevin_node_cc

void register_node_type_sh_kevinnode()
{
  namespace file_ns = blender::nodes::node_shader_kevin_node_cc;

  static bNodeType ntype;

  // Initialise the base node
  sh_node_type_base(&ntype, SH_NODE_KEVIN_NODE, "Kevin Node Name", NODE_CLASS_CONVERTER);

  // Initialise the storage data
  node_type_init(&ntype, file_ns::node_shader_init_kevin_node);

  // Set functions for freeing and copying the storage
  // node_free_standard_storage and node_copy_standard_storage are default free/copy functions for
  // any generic storage unless some fancy functionality is required
  node_type_storage(&ntype, "KevinNodeData", node_free_standard_storage, node_copy_standard_storage);

  ntype.draw_buttons = file_ns::node_shader_buts_kevinnode;

  nodeRegisterType(&ntype);
}