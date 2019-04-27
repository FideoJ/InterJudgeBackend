## Introdution
- inter-judge: A distributed online judge server based on ZeroMQ, Protocol Buffers, etc.
- [inter-judge-agent](https://github.com/FideoJ/InterJudgeAgent): The HTTP interface of inter judge.
- [inter-judge-backend](https://github.com/FideoJ/InterJudgeBackend): The set of backend components of inter judge.
## Usage
### Create a judge task
```
curl -X POST  \
  http://localhost:1212/judge \
  -H 'cache-control: no-cache' \
  -H 'content-type: application/json' \
  -d '{
    "sub_id": 123,
    "prob_id": 1024,
    "file_provider": "cool",
    "sub_src_filename": ["echo.cpp"],
    "sub_header_filename": [],
    "prob_src_filename": [],
    "prob_header_filename": [],
    "test_case_id": [1, 2, 3],
    "max_cpu_time": 1000,
    "max_memory": 1048576
}
'
```

```
{
  "overview": {
    "judge_id": "9f49e709-9393-4e45-bd4e-a6d21d850637",
    "completed": true,
    "start_time": "2019-03-27 07:24:32.157",
    "end_time": "2019-03-27 07:25:35.157",
    "result": "WRONG_ANSWER"
  },
  "compiler_gpp": {
    "completed": true,
    "start_time": "2019-03-27 07:24:32.157",
    "end_time": "2019-03-27 07:24:35.157",
    "result": "SUCCESS",
    "detail": ""
  },
  "tester": {
    "completed": true,
    "start_time": "2019-03-27 07:25:32.157",
    "end_time": "2019-03-27 07:25:35.157",
    "test_case": [{
        "cpu_time": 500,
        "result": "SUCCESS",
        "memory": 12836864,
        "signal": 0,
        "exit_code": 0,
        "test_case_id": 1
      },
      {
        "cpu_time": 500,
        "result": "WRONG_ANSWER",
        "memory": 12836864,
        "signal": 0,
        "exit_code": 0,
        "test_case_id": 2
      }
    ]
  },
  "file_provider": {
    "client": "cool",
    "compiler_gpp": "cool",
    "tester": "cool"
  },
  "origin_req": {
    "sub_id": 123,
    "prob_id": 1024,
    "provider_name": "cool",
    "src_filename": ["echo.cpp"],
    "header_filename": [],
    "test_case_id": [1, 2, 3],
    "max_cpu_time": 1000,
    "max_memory": 1048576
  }
}
```

### Query a judge task
```
curl http://localhost:1212/judge?judge_id=123
```
```
{
  "overview": {
    "judge_id": "9f49e709-9393-4e45-bd4e-a6d21d850637",
    "completed": true,
    "start_time": "2019-03-27 07:24:32.157",
    "end_time": "2019-03-27 07:25:35.157",
    "result": "WRONG_ANSWER"
  },
  "compiler_gpp": {
    "completed": true,
    "start_time": "2019-03-27 07:24:32.157",
    "end_time": "2019-03-27 07:24:35.157",
    "result": "SUCCESS",
    "detail": ""
  },
  "tester": {
    "completed": true,
    "start_time": "2019-03-27 07:25:32.157",
    "end_time": "2019-03-27 07:25:35.157",
    "test_case": [{
        "cpu_time": 500,
        "result": "SUCCESS",
        "memory": 12836864,
        "signal": 0,
        "exit_code": 0,
        "test_case_id": 1
      },
      {
        "cpu_time": 500,
        "result": "WRONG_ANSWER",
        "memory": 12836864,
        "signal": 0,
        "exit_code": 0,
        "test_case_id": 2
      }
    ]
  },
  "file_provider": {
    "client": "cool",
    "compiler_gpp": "cool",
    "tester": "cool"
  },
  "origin_req": {
    "sub_id": 123,
    "prob_id": 1024,
    "provider_name": "cool",
    "src_filename": ["echo.cpp"],
    "header_filename": [],
    "test_case_id": [1, 2, 3],
    "max_cpu_time": 1000,
    "max_memory": 1048576
  }
}
```

### Delete a judge task (optionally)
```
curl -X DELETE \
  'http://localhost:1212/judge?judge_id=123'
```

## References
- [Majordomo Protocol](https://rfc.zeromq.org/spec:7/MDP/)