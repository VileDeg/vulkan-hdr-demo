from cubemap_splitter import split_cubemap

# Automatically determine format and create new directory with images at original image location
split_cubemap("assets\\images\\castle_cellar.hdr")

# Specify format and write to user defined directory
#split_cubemap("C:\\Users\\paulb\\Desktop\\cubemap_formats\\cubemap.png", format_type=1, output_directory="c:/users/paulb/new_splits")