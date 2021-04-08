# Chord_Node
P2P 네트워킹을 위한 소켓 API 기반 Chord Node 구현




## 1. 요구사항  
* 노드의 Leave에 따른 핑거 테이블 및 파일 참조 정보의 정확한 갱신
* 파일 삭제 시 파일의 참조 정보의 정확한 갱신
* Ping-Pong기능 및 결과에 따른 핑거 테이블의 정확한 갱신
* 여러 노드가 동시 접속 시 안정적 동작
* SHA-1등의 해시 기법을 이용한 충돌 방지
* 노드가 비정상적으로 연결 해제할 때 네트워크의 복구 및 가동

## 2. 시퀀스 다이어 그램  
### [File Add]  
![image](https://user-images.githubusercontent.com/52437364/114031944-d0d4aa00-98b6-11eb-9838-28179fd47685.png)
### [File Delete]
![image](https://user-images.githubusercontent.com/52437364/114032290-24df8e80-98b7-11eb-8410-b2c589756e54.png)
### [File Search(File Download)]
![image](https://user-images.githubusercontent.com/52437364/114032467-4b052e80-98b7-11eb-9850-135b82d81a88.png)
  


## 3. 프로그램 테스트  
![image](https://user-images.githubusercontent.com/52437364/114032880-b7802d80-98b7-11eb-98a0-c75645e844ba.png)
  


## 4. 성능 평가  
![image](https://user-images.githubusercontent.com/52437364/114033616-5f95f680-98b8-11eb-8253-bf36a87794a1.png)
![image](https://user-images.githubusercontent.com/52437364/114033741-7b010180-98b8-11eb-824a-276ef8f3a085.png)
![image](https://user-images.githubusercontent.com/52437364/114034152-e519a680-98b8-11eb-98a6-051f818ea949.png)
![image](https://user-images.githubusercontent.com/52437364/114034271-febaee00-98b8-11eb-835b-a499f2b67f1d.png)
