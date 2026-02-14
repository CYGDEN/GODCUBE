#define NOMINMAX
#include <windows.h>
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstdio>

static const float BG[3]={.05f,.05f,.07f};
static const float CUBE_COL[3]={0,1,0};
static const float WIRE_COL[3]={0,.6f,0};
static const float TEXT_COL[3]={1,1,0};
static const float GLOW_COL[3]={.2f,1,.2f};
static const float SHADOW_COL[3]={.4f,.4f,0};

static const float FREQS[]={261.63f,293.66f,329.63f,349.23f,392.f,440.f,493.88f,523.25f,587.33f,659.25f,698.46f,783.99f};
static const char*NAMES[]={"C4","D4","E4","F4","G4","A4","B4","C5","D5","E5","F5","G5"};
static const int NUM_NOTES=12;
static const int PENTA[]={0,2,4,7,9,12-1};
static const int NUM_PENTA=5;

static float angX=25,angY=-35,angZ=0;
static float velX=0,velY=0,velZ=0;
static const float DRAG=.92f;
static const float THRESH=.6f;
static int mX=0,mY=0;
static bool mDown=false,rDown=false;
static bool rotating=false;
static int note=0;
static int lastNote=-1;
static int noteTimer=0;
static const int NOTE_GAP=6;
static int frame=0;
static int lastTime=0;
static const int FPS=60;
static const int DT=1000/FPS;
static float cubeScale=1.f;
static float targetScale=1.f;
static float noteFlash=0;
static float trailAngX[8],trailAngY[8];
static int trailIdx=0;
static float pulsePhase=0;
static float cubeHue=.33f;
static float targetHue=.33f;
static int combo=0;
static float comboFlash=0;
static float particleX[32],particleY[32],particleZ[32];
static float particleVX[32],particleVY[32],particleVZ[32];
static float particleLife[32];
static int particleCount=0;
static float camDist=5.f;
static float targetCamDist=5.f;
static bool autoSpin=false;
static float autoSpinSpeed=.5f;
static int mode=0;
static float modeFlash=0;
static const char*MODE_NAMES[]={"CHROMATIC","PENTATONIC","RANDOM"};
static float bgPulse=0;
static int totalNotes=0;
static float faceColors[6][3];
static bool lightsOn=true;
static float shakeX=0,shakeY=0;
static float shakeDecay=0;

struct Beep_t{DWORD f,d;};

DWORD WINAPI BeepProc(LPVOID p){
    Beep_t*b=(Beep_t*)p;
    Beep(b->f,b->d);
    delete b;
    return 0;
}

void beepAsync(int idx){
    DWORD f=(DWORD)FREQS[idx%NUM_NOTES];
    if(idx>=NUM_NOTES)f*=2;
    Beep_t*b=new Beep_t{f,55};
    HANDLE h=CreateThread(0,0,BeepProc,b,0,0);
    if(h)CloseHandle(h);
}

void hsvToRgb(float h,float s,float v,float*r,float*g,float*b){
    int i=(int)(h*6)%6;
    float f=h*6-floorf(h*6);
    float p=v*(1-s),q=v*(1-f*s),t=v*(1-(1-f)*s);
    switch(i){
        case 0:*r=v;*g=t;*b=p;break;
        case 1:*r=q;*g=v;*b=p;break;
        case 2:*r=p;*g=v;*b=t;break;
        case 3:*r=p;*g=q;*b=v;break;
        case 4:*r=t;*g=p;*b=v;break;
        default:*r=v;*g=p;*b=q;
    }
}

void spawnParticles(int n){
    for(int i=0;i<n&&particleCount<32;i++){
        int p=particleCount++;
        particleX[p]=((rand()%100)-50)*.02f;
        particleY[p]=((rand()%100)-50)*.02f;
        particleZ[p]=((rand()%100)-50)*.02f;
        particleVX[p]=((rand()%100)-50)*.03f;
        particleVY[p]=((rand()%100)-50)*.03f+.05f;
        particleVZ[p]=((rand()%100)-50)*.03f;
        particleLife[p]=1.f;
    }
}

int pickNote(){
    switch(mode){
        case 0:{
            int n=lastNote+1+(rand()%3)-1;
            if(n<0)n=0;if(n>=NUM_NOTES)n=NUM_NOTES-1;
            return n;
        }
        case 1:return PENTA[rand()%NUM_PENTA];
        default:return rand()%NUM_NOTES;
    }
}

void initFaceColors(){
    for(int i=0;i<6;i++)
        hsvToRgb(fmodf(cubeHue+i*.1f,1.f),.8f,.9f,&faceColors[i][0],&faceColors[i][1],&faceColors[i][2]);
}

void drawText(float x,float y,float z,const char*s,void*font=GLUT_BITMAP_8_BY_13){
    glRasterPos3f(x,y,z);
    for(;*s;s++)glutBitmapCharacter(font,*s);
}

void drawCube(float sz){
    float h=sz*.5f;
    float faces[6][4][3]={
        {{-h,-h,h},{h,-h,h},{h,h,h},{-h,h,h}},
        {{h,-h,-h},{-h,-h,-h},{-h,h,-h},{h,h,-h}},
        {{-h,h,h},{h,h,h},{h,h,-h},{-h,h,-h}},
        {{-h,-h,-h},{h,-h,-h},{h,-h,h},{-h,-h,h}},
        {{h,-h,h},{h,-h,-h},{h,h,-h},{h,h,h}},
        {{-h,-h,-h},{-h,-h,h},{-h,h,h},{-h,h,-h}}
    };
    float normals[6][3]={{0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}};
    for(int i=0;i<6;i++){
        float r=faceColors[i][0],g=faceColors[i][1],b=faceColors[i][2];
        float flash=noteFlash*.3f;
        glColor3f(fminf(r+flash,1),fminf(g+flash,1),fminf(b+flash,1));
        glNormal3fv(normals[i]);
        glBegin(GL_QUADS);
        for(int j=0;j<4;j++)glVertex3fv(faces[i][j]);
        glEnd();
    }
    glColor3f(.1f,.1f,.1f);
    glLineWidth(2.f);
    for(int i=0;i<6;i++){
        glBegin(GL_LINE_LOOP);
        for(int j=0;j<4;j++)glVertex3fv(faces[i][j]);
        glEnd();
    }
}

void display(){
    float bp=bgPulse*.03f;
    glClearColor(BG[0]+bp,BG[1]+bp,BG[2]+bp*1.5f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(shakeX,shakeY,camDist,shakeX*.5f,shakeY*.5f,0,0,1,0);

    glPushMatrix();
    if(noteFlash>.1f){
        glDisable(GL_DEPTH_TEST);
        for(int t=7;t>=1;t--){
            float a=t*.12f*noteFlash*.15f;
            float sc=cubeScale*(1+t*.04f);
            glPushMatrix();
            glRotatef(trailAngX[(trailIdx-t+8)&7],1,0,0);
            glRotatef(trailAngY[(trailIdx-t+8)&7],0,1,0);
            glRotatef(angZ,0,0,1);
            glScalef(sc,sc,sc);
            glColor4f(GLOW_COL[0],GLOW_COL[1],GLOW_COL[2],a);
            glutWireCube(1.02f);
            glPopMatrix();
        }
        glEnable(GL_DEPTH_TEST);
    }

    glRotatef(angX,1,0,0);
    glRotatef(angY,0,1,0);
    glRotatef(angZ,0,0,1);

    float s=cubeScale+sinf(pulsePhase)*.015f;
    glScalef(s,s,s);

    if(lightsOn){
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        float lp[]={2,3,4,1};
        float la[]={.15f,.15f,.15f,1};
        float ld[]={.9f,.9f,.9f,1};
        glLightfv(GL_LIGHT0,GL_POSITION,lp);
        glLightfv(GL_LIGHT0,GL_AMBIENT,la);
        glLightfv(GL_LIGHT0,GL_DIFFUSE,ld);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    }

    drawCube(1.f);

    if(lightsOn){
        glDisable(GL_LIGHTING);
        glDisable(GL_COLOR_MATERIAL);
    }

    glPopMatrix();

    glDisable(GL_DEPTH_TEST);
    if(particleCount>0){
        glPointSize(3.f);
        glBegin(GL_POINTS);
        for(int i=0;i<particleCount;i++){
            float r,g,b;
            hsvToRgb(fmodf(cubeHue+particleLife[i]*.3f,1.f),.8f,particleLife[i],&r,&g,&b);
            glColor3f(r,g,b);
            glVertex3f(particleX[i],particleY[i],particleZ[i]);
        }
        glEnd();
    }
    glEnable(GL_DEPTH_TEST);

    glDisable(GL_DEPTH_TEST);
    glColor3fv(TEXT_COL);
    char buf[128];

    snprintf(buf,sizeof buf,"NOTE: %s  MODE: %s  COMBO: %d  TOTAL: %d",
        NAMES[note],MODE_NAMES[mode],combo,totalNotes);
    drawText(-3.6f,2.2f,-1,buf);

    snprintf(buf,sizeof buf,"DRAG:PLAY  R:RESET  M:MODE  L:LIGHT  SPACE:AUTOSPIN  Q:QUIT");
    drawText(-3.6f,-2.5f,-1,buf,GLUT_BITMAP_HELVETICA_10);

    if(comboFlash>.1f&&combo>=5){
        float cg=.5f+comboFlash*.5f;
        glColor3f(1,cg,0);
        snprintf(buf,sizeof buf,"x%d COMBO!",combo);
        drawText(-0.5f,1.7f,-1,buf,GLUT_BITMAP_HELVETICA_18);
    }

    if(modeFlash>.1f){
        glColor3f(1,1,modeFlash);
        snprintf(buf,sizeof buf,">> %s <<",MODE_NAMES[mode]);
        drawText(-0.8f,0,-1,buf,GLUT_BITMAP_HELVETICA_18);
    }

    glEnable(GL_DEPTH_TEST);
    glutSwapBuffers();
}

void reshape(int w,int h){
    if(!h)h=1;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(55,(float)w/h,.1,100);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}

void mouse(int b,int st,int x,int y){
    if(b==GLUT_LEFT_BUTTON){
        mDown=(st==GLUT_DOWN);
        mX=x;mY=y;
        if(!mDown){rotating=false;combo=0;}
    }
    if(b==GLUT_RIGHT_BUTTON){
        rDown=(st==GLUT_DOWN);
        mX=x;mY=y;
    }
    if(b==3)targetCamDist=fmaxf(2,targetCamDist-.3f);
    if(b==4)targetCamDist=fminf(12,targetCamDist+.3f);
}

void motion(int x,int y){
    int dx=x-mX,dy=y-mY;
    if(mDown){
        velX+=dy*.22f;
        velY+=dx*.22f;
        rotating=true;
    }
    if(rDown){
        velZ+=dx*.22f;
    }
    mX=x;mY=y;
}

void timer(int){
    int now=glutGet(GLUT_ELAPSED_TIME);
    int elapsed=now-lastTime;

    if(elapsed>=DT){
        lastTime=now;

        if(autoSpin){
            velY+=autoSpinSpeed;
            rotating=true;
        }

        angX+=velX;angY+=velY;angZ+=velZ;
        velX*=DRAG;velY*=DRAG;velZ*=DRAG;
        if(fabsf(velX)<.01f)velX=0;
        if(fabsf(velY)<.01f)velY=0;
        if(fabsf(velZ)<.01f)velZ=0;

        trailAngX[trailIdx&7]=angX;
        trailAngY[trailIdx&7]=angY;
        trailIdx++;

        cubeScale+=(targetScale-cubeScale)*.15f;
        camDist+=(targetCamDist-camDist)*.1f;
        cubeHue+=(targetHue-cubeHue)*.08f;
        initFaceColors();

        pulsePhase+=.08f;
        noteFlash*=.88f;
        comboFlash*=.92f;
        modeFlash*=.9f;
        bgPulse*=.9f;

        shakeX*=.85f;shakeY*=.85f;
        shakeDecay*=.9f;

        for(int i=0;i<particleCount;){
            particleX[i]+=particleVX[i];
            particleY[i]+=particleVY[i];
            particleZ[i]+=particleVZ[i];
            particleVY[i]-=.002f;
            particleLife[i]-=.025f;
            if(particleLife[i]<=0){
                particleCount--;
                particleX[i]=particleX[particleCount];
                particleY[i]=particleY[particleCount];
                particleZ[i]=particleZ[particleCount];
                particleVX[i]=particleVX[particleCount];
                particleVY[i]=particleVY[particleCount];
                particleVZ[i]=particleVZ[particleCount];
                particleLife[i]=particleLife[particleCount];
            }else i++;
        }

        float spd=sqrtf(velX*velX+velY*velY+velZ*velZ);
        if(rotating&&spd>THRESH){
            if(frame-noteTimer>=NOTE_GAP){
                note=pickNote();
                lastNote=note;
                beepAsync(note);
                noteTimer=frame;
                noteFlash=1;
                targetScale=1.08f;
                combo++;
                totalNotes++;
                targetHue=fmodf(targetHue+.07f,1.f);
                bgPulse=1;
                spawnParticles(4+combo/3);
                if(combo>=5)comboFlash=1;
                if(combo>=10){
                    shakeX=((rand()%100)-50)*.002f;
                    shakeY=((rand()%100)-50)*.002f;
                }
            }
        }else{
            targetScale=1.f;
        }

        if(!rotating&&!autoSpin&&spd<.05f)combo=0;

        frame++;
        glutPostRedisplay();
    }
    glutTimerFunc(DT,timer,0);
}

void keyboard(unsigned char k,int,int){
    switch(k){
        case 27:case'q':case'Q':exit(0);
        case'r':case'R':
            angX=25;angY=-35;angZ=0;
            velX=velY=velZ=0;
            cubeHue=.33f;targetHue=.33f;
            combo=0;totalNotes=0;
            particleCount=0;
            camDist=targetCamDist=5;
            break;
        case'm':case'M':
            mode=(mode+1)%3;
            modeFlash=1;
            break;
        case'l':case'L':
            lightsOn=!lightsOn;
            break;
        case' ':
            autoSpin=!autoSpin;
            break;
        case'+':case'=':
            autoSpinSpeed=fminf(3,autoSpinSpeed+.2f);
            break;
        case'-':case'_':
            autoSpinSpeed=fmaxf(.1f,autoSpinSpeed-.2f);
            break;
    }
}

int main(int argc,char**argv){
    srand((unsigned)time(0));
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
    glutInitWindowSize(800,600);
    glutCreateWindow("TempleOS Note Cube v3.0");

    glClearColor(BG[0],BG[1],BG[2],1);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    cubeHue=.33f;targetHue=.33f;
    initFaceColors();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);

    lastTime=glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(DT,timer,0);

    printf("TempleOS Note Cube v3.0\n");
    printf("LMB+DRAG: Rotate & play | RMB+DRAG: Z-axis\n");
    printf("M: Mode | L: Lighting | SPACE: Auto-spin\n");
    printf("+/-: Spin speed | R: Reset | Q: Quit\n");
    printf("Praise God.\n\n");

    glutMainLoop();
    return 0;
}