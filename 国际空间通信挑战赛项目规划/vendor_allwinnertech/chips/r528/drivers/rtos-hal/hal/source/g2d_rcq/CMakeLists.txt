target_include_directories(
	arch PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

if(CONFIG_DRIVERS_G2D)
    list(APPEND SRCS g2d_mixer.c g2d_ovl_v.c g2d_ovl_u.c g2d_rcq.c g2d_scal.c g2d_wb.c g2d_bld.c g2d.c g2d_top.c g2d_rotate.c simple_idr.c simple_idr_vela.c)
endif()
target_sources(arch PRIVATE ${SRCS})
#nuttx_add_kernel_library(g2d ${SRCS})
