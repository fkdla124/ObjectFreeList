# ObjectFreeList<T>
* 게임서버 프로젝트에서 여러 오브젝트의 메모리 풀 개념으로 사용할 프리리스트

# 성능 테스트
* [파이터 미니프로젝트](https://github.com/fkdla124/FighterServer) 에서 직렬화버퍼 new를 직접만든 프리리리스트 alloc() 으로 변경
* new에비해 약 2.6배 빠른 결과

### new
![new](<./[2코스 과제] ObjectFreeList/new_delete.png>)

### alloc
![alloc](<./[2코스 과제] ObjectFreeList/alloc.png>)
