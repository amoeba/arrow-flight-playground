FROM python:3.11

WORKDIR /work

ADD ./requirements.txt ./
ADD ./*.py ./

RUN --mount=type=cache,target=/root/.cache/pip \
    python -m pip install -r requirements.txt

CMD [ "python3", "simulate_traffic.py" ]
