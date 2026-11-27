FROM condaforge/miniforge3:latest
RUN mamba install -y -c conda-forge jupyter-sysml-kernel=0.63.0 nbconvert cairosvg && mamba clean -afy
WORKDIR /work
